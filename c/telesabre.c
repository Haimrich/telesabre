#include "telesabre.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "circuit.h"
#include "config.h"
#include "device.h"
#include "layout.h"
#include "report.h"
#include "utils.h"
#include "graph.h"


telesabre_t* telesabre_init(config_t* config, device_t* device, circuit_t* circuit) {
    telesabre_t* ts = malloc(sizeof(telesabre_t));

    ts->config = config;
    ts->device = device;
    ts->circuit = circuit;
    
    // Inizialize circuit front
    ts->gate_num_remaining_parents = malloc(sizeof(size_t) * circuit->num_gates);
    for (size_t g = 0; g < circuit->num_gates; g++) 
        ts->gate_num_remaining_parents[g] = circuit->gates[g].num_parents;

    ts->front = malloc(sizeof(size_t) * circuit->num_gates);
    ts->front_size = 0;
    for (size_t g = 0; g < circuit->num_gates; g++) {
        if (ts->gate_num_remaining_parents[g] == 0) {
            ts->front[ts->front_size++] = g;
        }
    }

    // Inizialize layout
    ts->layout = initial_layout(device, circuit, config);

    // Usage Penalties
    ts->usage_penalties = malloc(sizeof(float) * device->num_qubits);
    for (pqubit_t p = 0; p < device->num_qubits; p++) 
        ts->usage_penalties[p] = 1.0f;
    ts->usage_penalties_reset_counter = config->usage_penalties_reset_interval;

    // Other state variables
    ts->it = 0;
    ts->it_without_progress = 0;

    ts->safety_valve_activated = false;
    ts->last_progress_layout = layout_copy(ts->layout);

    // Array of candidate operations
    ts->candidate_ops = NULL;
    ts->candidate_ops_energies = NULL;
    ts->num_candidate_ops = 0;
    ts->candidate_ops_capacity = 0;

    // Remaining slices
    ts->remaining_slices = malloc(sizeof(size_t) * circuit->num_gates);
    ts->remaining_slices_ptr = malloc(sizeof(size_t) * (circuit->num_gates + 1));
    ts->num_remaining_slices = 0;

    // Applied gates
    ts->applied_gates = malloc(sizeof(int) * circuit->num_gates);
    ts->num_applied_gates = 0;

    // Attraction paths
    ts->attraction_paths = NULL;
    ts->num_attraction_paths = 0;
    ts->attraction_paths_capacity = 0;

    // Traversed communication qubits
    ts->traversed_comm_qubits = NULL;
    ts->num_traversed_comm_qubits = 0;
    ts->traversed_comm_qubits_capacity = 0;

    // Nearest free qubits
    ts->nearest_free_qubits = NULL;
    ts->num_nearest_free_qubits = 0;
    ts->nearest_free_qubits_capacity = 0;

    ts->result = (result_t){0};

    ts->report = report_new();

    return ts;
}


void telesabre_safety_valve_check(telesabre_t *ts) {
    if (ts->it_without_progress > ts->config->safety_valve_iters && !ts->safety_valve_activated) {
        ts->safety_valve_activated = true;
        layout_free(ts->layout);
        ts->layout = layout_copy(ts->last_progress_layout);
        ts->result = ts->last_progress_result;
        printf("Safety valve activated at iteration %d\n", ts->it);
        ts->result.num_deadlocks++;
    }
}


void telesabre_execute_front_gate(telesabre_t* ts, size_t front_gate_idx) {
    const gate_t* gate = &ts->circuit->gates[ts->front[front_gate_idx]];

    // Debug Print
    printf(H3COL"  Executing gate "CRESET"%03zu = %s(", ts->front[front_gate_idx], gate->type);
    for (int j = 0; j < gate->num_target_qubits; j++) {
        printf("%d", gate->target_qubits[j]);
        if (j < gate->num_target_qubits - 1) printf(", ");
    }
    printf(")\n");
    
    // Update Usage Penalties
    for (vqubit_t v = 0; v < gate->num_target_qubits; v++) {
        pqubit_t phys = layout_get_phys(ts->layout, gate->target_qubits[v]);
        ts->usage_penalties[phys] += ts->config->usage_penalty;
    }

    // Mark as executed
    ts->gate_num_remaining_parents[gate->id] = (size_t)-1;

    // Remove from front
    if (front_gate_idx < ts->front_size - 1) {
        ts->front[front_gate_idx] = ts->front[ts->front_size - 1];
    }
    ts->front_size--;

    // Update front
    for (size_t j = 0; j < gate->num_children; j++) {
        size_t child_id = gate->children_id[j];
        ts->gate_num_remaining_parents[child_id]--;
        if (ts->gate_num_remaining_parents[child_id] == 0) {
            if (child_id == 0) {
                printf("adding node 0 again wtf\n");
                exit(1);
            }
            ts->front[ts->front_size++] = child_id;
        }
    }

    // Mark remaining circuit slices for update
    ts->slices_outdated = true;
}


void telesabre_made_progress(telesabre_t* ts) {
    ts->it_without_progress = 0;
    if (ts->safety_valve_activated) {
        ts->safety_valve_activated = false;
        ts->result.num_deadlocks++;
    }
    layout_free(ts->last_progress_layout);
    ts->last_progress_layout = layout_copy(ts->layout);
    ts->last_progress_result = ts->result;
}


void telesabre_calculate_attraction_paths(telesabre_t *ts) {
    ts->num_attraction_paths = 0;

    if (ts->front_size > ts->attraction_paths_capacity) {
        ts->attraction_paths_capacity = ts->front_size;
        ts->attraction_paths = realloc(ts->attraction_paths, sizeof(path_t*) * ts->attraction_paths_capacity);
    }

    for (int i = 0; i < ts->front_size; i++) {
        const gate_t* gate = &ts->circuit->gates[ts->front[i]];
        if (!layout_gate_is_separated(ts->layout, gate)) continue;
        
        size_t separated_node_ids[2] = {0};
        pqubit_t node_id_to_phys[2] = {0};

        graph_t* contracted_graph = telesabre_build_contracted_graph_for_pair(ts, ts->layout, gate, separated_node_ids, node_id_to_phys);

        int *node_id_translation = malloc(sizeof(int) * contracted_graph->num_nodes);
        memcpy(node_id_translation, ts->device->comm_qubits, sizeof(int) * ts->device->num_comm_qubits);
        for (int j = 0; j < contracted_graph->num_nodes - ts->device->num_comm_qubits; j++) {
            node_id_translation[j + ts->device->num_comm_qubits] = node_id_to_phys[j];
        }
       // graph_print(contracted_graph, node_id_translation);
        free(node_id_translation);
        
        int src = separated_node_ids[0];
        int dst = separated_node_ids[1];

        path_t* shortest_path = graph_dijkstra(contracted_graph, src, dst);

        // Translate internal graph ids to physical qubit id
        for (int j = 0; j < shortest_path->length; j++) {
            int internal_id = shortest_path->nodes[j];
            if (internal_id < ts->device->num_comm_qubits) {
                shortest_path->nodes[j] = ts->device->comm_qubits[internal_id];
            } else {
                internal_id = internal_id - ts->device->num_comm_qubits;
                shortest_path->nodes[j] = node_id_to_phys[internal_id];
            }
        }
        ts->attraction_paths[ts->num_attraction_paths++] = shortest_path;

        graph_free(contracted_graph);
    }

    // Print needed comm. qubits
    printf(H2COL"  Needed Paths: "CRESET"%zu\n", ts->num_attraction_paths);
    for (int i = 0; i < ts->num_attraction_paths; i++) {
        printf("    Path %d: ", i);
        for (int j = 0; j < ts->attraction_paths[i]->length; j++) {
            printf("%d ", ts->attraction_paths[i]->nodes[j]);
        }
        printf("\n");
    }
}


void telesabre_collect_traversed_comm_qubits(telesabre_t *ts) {
    ts->num_traversed_comm_qubits = 0;

    int potential_num_traversed_comm_qubits = 0;
    for (int i = 0; i < ts->num_attraction_paths; i++) {
        const path_t* shortest_path = ts->attraction_paths[i];
        potential_num_traversed_comm_qubits += shortest_path->length;
    }
    if (potential_num_traversed_comm_qubits > ts->traversed_comm_qubits_capacity) {
        ts->traversed_comm_qubits_capacity = potential_num_traversed_comm_qubits;
        ts->traversed_comm_qubits = realloc(ts->traversed_comm_qubits, sizeof(pqubit_t) * ts->traversed_comm_qubits_capacity);
    }

    for (int i = 0; i < ts->num_attraction_paths; i++) {
        const path_t* shortest_path = ts->attraction_paths[i];

        // Collect needed communication qubits
        for (int j = 0; j < shortest_path->length; j++) {
            pqubit_t pc = shortest_path->nodes[j];
            if (ts->device->qubit_is_comm[pc]) {
                ts->traversed_comm_qubits[ts->num_traversed_comm_qubits++] = pc;
            }
        }
    }

    printf(H2COL"  Needed communication qubits: "CRESET);
    for (int j = 0; j < ts->num_traversed_comm_qubits; j++)
        printf("%d ", ts->traversed_comm_qubits[j]);
    printf("\n");
}


void telesabre_collect_nearest_free_qubits(telesabre_t *ts) {
    ts->num_nearest_free_qubits = 0;

    if (ts->num_traversed_comm_qubits > ts->nearest_free_qubits_capacity) {
        ts->nearest_free_qubits_capacity = ts->num_traversed_comm_qubits;
        ts->nearest_free_qubits = realloc(ts->nearest_free_qubits, sizeof(pqubit_t) * ts->nearest_free_qubits_capacity);
    }

    // We might want to make a set in future
    for (int i = 0; i < ts->num_traversed_comm_qubits; i++) {
        const pqubit_t pc = ts->traversed_comm_qubits[i];
        pqubit_t nearest_free_qubit = layout_get_nearest_free_qubit(ts->layout, ts->device->comm_qubit_node_id[pc]);
        if (nearest_free_qubit != -1) {
            ts->nearest_free_qubits[ts->num_nearest_free_qubits++] = nearest_free_qubit;
        }
            
    }

    printf(H2COL"  Needed nearest free qubits: "CRESET);
    for (int j = 0; j < ts->num_nearest_free_qubits; j++)
        printf("%d ", ts->nearest_free_qubits[j]);
    printf("\n");
}


void telesabre_slice_remaining_circuit(telesabre_t *ts) {
    size_t num_gates = ts->circuit->num_gates;
    size_t *rem_parents = malloc(sizeof(size_t) * num_gates);
    memcpy(rem_parents, ts->gate_num_remaining_parents, sizeof(size_t) * num_gates);

    // Queue for ready gates
    size_t *queue = malloc(sizeof(size_t) * num_gates);
    size_t q_head = 0, q_tail = 0;

    // Initialize queue with all gates with in-degree 0
    for (size_t i = 0; i < num_gates; ++i) {
        if (rem_parents[i] == 0) {
            queue[q_tail++] = i;
        }
    }

    size_t num_slices = 0;
    size_t gate_out_idx = 0;

    while (q_head < q_tail) {
        // Mark the start of this slice
        ts->remaining_slices_ptr[num_slices] = gate_out_idx;
        size_t old_q_tail = q_tail;

        for (; q_head < old_q_tail; ++q_head) {
            size_t g = queue[q_head];
            if (rem_parents[g] == (size_t)-1) continue;
            size_t curr = g;

            // Bypass single-qubit gates
            while (
                curr < num_gates &&
                ts->circuit->gates[curr].num_target_qubits == 1 &&
                rem_parents[curr] != (size_t)-1
            ) {
                rem_parents[curr] = (size_t)-1;
                if (ts->circuit->gates[curr].num_children == 1) {
                    size_t child = ts->circuit->gates[curr].children_id[0];
                    if (rem_parents[child] > 0 && rem_parents[child] != (size_t)-1) {
                        rem_parents[child]--;
                        if (rem_parents[child] == 0) {
                            queue[q_tail++] = child;
                        }
                    }
                    curr = child;
                } else {
                    // No children, so stop bypassing
                    break;
                }
            }
            if (curr >= num_gates || rem_parents[curr] == (size_t)-1) continue;

            // Add two-qubit (or multi-qubit) gate to the slice
            rem_parents[curr] = (size_t)-1;
            ts->remaining_slices[gate_out_idx++] = curr;
            for (size_t j = 0; j < ts->circuit->gates[curr].num_children; ++j) {
                size_t child = ts->circuit->gates[curr].children_id[j];
                if (rem_parents[child] > 0 && rem_parents[child] != (size_t)-1) {
                    rem_parents[child]--;
                    if (rem_parents[child] == 0) {
                        queue[q_tail++] = child;
                    }
                }
            }
        }
        // Only produce a slice if it contains any gates
        if (gate_out_idx > ts->remaining_slices_ptr[num_slices]) {
            num_slices++;
        }
    }
    ts->remaining_slices_ptr[num_slices] = gate_out_idx; // end pointer

    ts->num_remaining_slices = num_slices;

    free(rem_parents);
    free(queue);
}


float telesabre_evaluate_op_energy(telesabre_t* ts, const op_t* op) {
    // TODO: Add traffic

    // Copy layout and apply op
    layout_t* layout = layout_copy(ts->layout);
    if (op->type == OP_TELEPORT) {
        layout_apply_teleport(layout, op->qubits[0], op->qubits[1], op->qubits[2]);
    } else if (op->type == OP_SWAP) {
        layout_apply_swap(layout, op->qubits[0], op->qubits[1]);
    }

    float usage_penalty = ts->usage_penalties[op->qubits[0]];
    for (int i = 0; i < op_get_num_qubits(op); i++) {
        float new_penalty = ts->usage_penalties[op->qubits[i]];
        usage_penalty = new_penalty > usage_penalty ? new_penalty : usage_penalty;
    }

    float front_energy = 0.0f;
    float extended_energy = 0.0f;

    int extended_set_size = 0;

    for (int i = 0; i < ts->num_remaining_slices && extended_set_size < ts->config->extended_set_size; i++) {
        size_t slice_start = ts->remaining_slices_ptr[i];
        size_t slice_end = ts->remaining_slices_ptr[i + 1];

        for (size_t j = slice_start; j < slice_end && extended_set_size < ts->config->extended_set_size; j++) {
            const gate_t* gate = &ts->circuit->gates[ts->remaining_slices[j]];
            
            float gate_energy = 0.0f;
            vqubit_t v1 = gate->target_qubits[0];
            vqubit_t v2 = gate->target_qubits[1];
            pqubit_t p1 = layout_get_phys(layout, v1);
            pqubit_t p2 = layout_get_phys(layout, v2);
            core_t c1 = ts->device->phys_to_core[p1];
            core_t c2 = ts->device->phys_to_core[p2];
            if (c1 == c2) {
                gate_energy = device_get_distance(ts->device, p1, p2) * 2;
            } else {
                size_t separated_node_ids[2] = {0};
                pqubit_t node_id_to_phys[2] = {0};

                graph_t* contracted_graph = telesabre_build_contracted_graph_for_pair(
                    ts, layout, gate, separated_node_ids, node_id_to_phys
                );
                
                int src = separated_node_ids[0];
                int dst = separated_node_ids[1];

                path_t* shortest_path = graph_dijkstra(contracted_graph, src, dst);
                gate_energy = shortest_path->distance;
            }

            if (i == 0) {
                front_energy += gate_energy;
            } else {
                extended_energy += gate_energy;
                extended_set_size++;
            }

            // Consider only one gate in safety valve mode
            if (ts->safety_valve_activated) {
                if (front_energy <= 0.0f)
                    error("Error in energy calculation while safety valve is activated.");
                break; 
            }
        }

        if (ts->safety_valve_activated) break;
    }

    float energy = front_energy;
    if (!ts->safety_valve_activated) {
        energy /= ts->front_size;
    }
    if (extended_set_size > 0) {
        energy += ts->config->extended_set_factor * extended_energy / extended_set_size;
    }
    energy *= usage_penalty;

    layout_free(layout);

    //printf("Evaluating op: %d, energy: %.2f, front_energy: %.2f, extended_energy: %.2f, usage_penalty: %.2f\n",
    //       (op->type), energy, front_energy, extended_energy, usage_penalty);

    return energy;
}


void telesabre_add_candidate_op(telesabre_t* ts, const op_t* op) {
    if (ts->num_candidate_ops >= ts->candidate_ops_capacity) {
        ts->candidate_ops_capacity = (ts->candidate_ops_capacity == 0) ? 4 : ts->candidate_ops_capacity * 2;
        ts->candidate_ops = realloc(ts->candidate_ops, sizeof(op_t) * ts->candidate_ops_capacity);
        ts->candidate_ops_energies = realloc(ts->candidate_ops_energies, sizeof(float) * ts->candidate_ops_capacity);
    }

    ts->candidate_ops[ts->num_candidate_ops] = *op;

    int bonus = 0;
    if (op->type == OP_TELEPORT) {
        bonus = ts->config->teleport_bonus;
    } else if (op->type == OP_TELEGATE) {
        bonus = ts->config->telegate_bonus;
    }
    ts->candidate_ops_energies[ts->num_candidate_ops] = telesabre_evaluate_op_energy(ts, op) - bonus;

    ts->num_candidate_ops++;
}


void telesabre_collect_candidate_tele_ops(telesabre_t *ts) {
    const circuit_t* circuit = ts->circuit;
    const device_t* device = ts->device;
    const layout_t* layout = ts->layout;

    // Find feasible inter-core operations
    ts->num_candidate_ops = 0;

    for (int i = 0; i < ts->num_attraction_paths; i++) {
        const gate_t* gate = &circuit->gates[ts->front[i]];
        const path_t* shortest_path = ts->attraction_paths[i];

        // Check if telegate is possible
        if (shortest_path->length == 4) {
            pqubit_t g1 = shortest_path->nodes[0]; // layout->get_phys(layout, gate->target_qubits[0]);
            pqubit_t m1 = shortest_path->nodes[1];
            pqubit_t m2 = shortest_path->nodes[2];
            pqubit_t g2 = shortest_path->nodes[3]; // layout->get_phys(layout, gate->target_qubits[1]);

            if (device->qubit_is_comm[m1] && layout_is_phys_free(layout, m1) && 
                device->qubit_is_comm[m2] && layout_is_phys_free(layout, m2) &&
                device_has_edge(device, g1, m1) && device_has_edge(device, m2, g2)) {

                // Add telegate operation
                op_t telegate_op = {.type = OP_TELEGATE, .qubits = {g1, m1, m2, g2}, .front_gate_idx = i};
                telesabre_add_candidate_op(ts, &telegate_op);
            }
        }

        // Check if teleport is possible
        if (shortest_path->length >= 3) {
            pqubit_t p1 = layout_get_phys(layout, gate->target_qubits[0]);
            pqubit_t p2 = layout_get_phys(layout, gate->target_qubits[1]);

            // Check forward direction
            pqubit_t fwd_source = shortest_path->nodes[0];
            pqubit_t fwd_mediator = shortest_path->nodes[1];
            pqubit_t fwd_target = shortest_path->nodes[2];
            core_t fwd_target_core = device->phys_to_core[fwd_target];

            if (fwd_source == p1 && device_has_edge(device, fwd_source, fwd_mediator) &&
                device->qubit_is_comm[fwd_mediator] && layout_is_phys_free(layout, fwd_mediator) &&
                device->qubit_is_comm[fwd_target] && layout_is_phys_free(layout, fwd_target) &&
                layout_get_core_remaining_capacity(layout, fwd_target_core) >= 2) {
                
                // Add teleport operation
                op_t teleport_op = {.type = OP_TELEPORT, .qubits = {fwd_source, fwd_mediator, fwd_target, 0}, .front_gate_idx = i};
                telesabre_add_candidate_op(ts, &teleport_op);
            }

            // Check reverse direction
            pqubit_t rev_source = shortest_path->nodes[shortest_path->length - 1];
            pqubit_t rev_mediator = shortest_path->nodes[shortest_path->length - 2];
            pqubit_t rev_target = shortest_path->nodes[shortest_path->length - 3];
            core_t rev_target_core = device->phys_to_core[rev_target];

            if (rev_source == p2 && device_has_edge(device, rev_source, rev_mediator) &&
                device->qubit_is_comm[rev_mediator] && layout_is_phys_free(layout, rev_mediator) &&
                device->qubit_is_comm[rev_target] && layout_is_phys_free(layout, rev_target) &&
                layout_get_core_remaining_capacity(layout, rev_target_core) >= 2) {
                
                // Add teleport operation
                op_t teleport_op = {.type = OP_TELEPORT, .qubits = {rev_source, rev_mediator, rev_target, 0}, .front_gate_idx = i};
                telesabre_add_candidate_op(ts, &teleport_op);
            }
        }
    }
}


void telesabre_collect_candidate_swap_ops(telesabre_t* ts) {
    const layout_t* layout = ts->layout;
    const device_t* device = ts->device;
    const circuit_t* circuit = ts->circuit;

    // Find feasible (and needed) swap operations
    for (int e = 0; e < device->num_edges; e++) {
        pqubit_t p1 = device->edges[e].p1;
        pqubit_t p2 = device->edges[e].p2;

        bool p1_is_busy = !layout_is_phys_free(layout, p1);
        bool p2_is_busy = !layout_is_phys_free(layout, p2);

        bool p1_is_needed_nearest_free = false;
        bool p1_is_in_front = false;
        if (p1_is_busy) {
            vqubit_t v1 = layout->phys_to_virt[p1];
            for (int j = 0; j < ts->front_size && !p1_is_in_front; j++) {
                const gate_t* gate = &circuit->gates[ts->front[j]];
                for (int k = 0; k < gate->num_target_qubits; k++) {
                    if (gate->target_qubits[k] == v1) {
                        p1_is_in_front = true;
                        break;
                    }
                }
            }
        } else {
            for (int j = 0; j < ts->num_nearest_free_qubits; j++) {
                if (ts->nearest_free_qubits[j] == p1) {
                    p1_is_needed_nearest_free = true;
                    break;
                }
            }
        }

        bool p2_is_needed_nearest_free = false;
        bool p2_is_in_front = false;
        if (p2_is_busy) {
            vqubit_t v2 = layout->phys_to_virt[p2];
            for (int j = 0; j < ts->front_size && !p2_is_in_front; j++) {
                const gate_t* gate = &circuit->gates[ts->front[j]];
                for (int k = 0; k < gate->num_target_qubits; k++) {
                    if (gate->target_qubits[k] == v2) {
                        p2_is_in_front = true;
                        break;
                    }
                }
            }
        } else {
            for (int j = 0; j < ts->num_nearest_free_qubits; j++) {
                if (ts->nearest_free_qubits[j] == p2) {
                    p2_is_needed_nearest_free = true;
                    break;
                }
            }
        }
        
        if ((p1_is_busy || p2_is_busy) && 
            (p1_is_in_front || p2_is_in_front || p1_is_needed_nearest_free || p2_is_needed_nearest_free)) {
            unsigned char reasons = 0;
            reasons = (p1_is_busy ? 1 : 0) | 
                      (p2_is_busy ? 2 : 0) | 
                      (p1_is_in_front ? 4 : 0) | 
                      (p2_is_in_front ? 8 : 0) | 
                      (p1_is_needed_nearest_free ? 16 : 0) | 
                      (p2_is_needed_nearest_free ? 32 : 0);
            op_t swap_op = {.type = OP_SWAP, .qubits = {p1, p2, 0, 0}, .front_gate_idx = -1, .reasons = reasons};
            telesabre_add_candidate_op(ts, &swap_op);
        }

    }
}


void telesabre_apply_candidate_op(telesabre_t *ts, const op_t *op) {
    if (op->type == OP_TELEPORT) 
    {
        layout_apply_teleport(ts->layout, op->qubits[0], op->qubits[1], op->qubits[2]);
        for (int i = 0; i < 3; i++) 
            ts->usage_penalties[op->qubits[i]] += ts->config->usage_penalty;

        ts->result.num_teledata++;
    } 
    else if (op->type == OP_SWAP) 
    {
        layout_apply_swap(ts->layout, op->qubits[0], op->qubits[1]);
        for (int i = 0; i < 2; i++) 
            ts->usage_penalties[op->qubits[i]] += ts->config->usage_penalty;
        ts->result.num_swaps++;
    } 
    else if (op->type == OP_TELEGATE) 
    {
        for (int i = 0; i < 4; i++)
            ts->usage_penalties[op->qubits[i]] += ts->config->usage_penalty;
        int front_gate_idx = op->front_gate_idx;
        ts->result.num_telegate++;
        telesabre_execute_front_gate(ts, front_gate_idx);
        telesabre_made_progress(ts);
    }

    printf(H2COL"  Applied operation: "CRESET);
    if (op->type == OP_TELEPORT) {
        printf("Teleport(%d, %d, %d)\n", op->qubits[0], op->qubits[1], op->qubits[2]);
    } else if (op->type == OP_SWAP) {
        printf("Swap(%d, %d)\n", op->qubits[0], op->qubits[1]);
    } else if (op->type == OP_TELEGATE) {
        printf("Telegate(%d, %d, %d, %d)\n", op->qubits[0], op->qubits[1], op->qubits[2], op->qubits[3]);
    }
}


graph_t* telesabre_build_contracted_graph_for_pair(
    const telesabre_t* ts,
    const layout_t* layout,
    const gate_t* gate, 
    size_t node_ids_out[2],
    pqubit_t* node_id_to_phys_out
) {
    const device_t* device = ts->device;
    int node_id = device->num_comm_qubits;

    for (int q = 0; q < gate->num_target_qubits; q++) {
        pqubit_t p = layout_get_phys(layout, gate->target_qubits[q]);
        if (device->qubit_is_comm[p]) {
            // Bad
            node_ids_out[q] = -1;
            for (int j = 0; j < device->num_comm_qubits; j++) {
                if (device->comm_qubits[j] == p) {
                    node_ids_out[q] = j;
                    break;
                }
            }
        } else {
            node_ids_out[q] = node_id++;
            node_id_to_phys_out[node_ids_out[q]-device->num_comm_qubits] = p;
        }
    }

    graph_t* graph = graph_new(node_id);

    pqubit_t start_qubit = layout_get_phys(layout, gate->target_qubits[0]);
    pqubit_t end_qubit = layout_get_phys(layout, gate->target_qubits[1]);
    
    // Add edges between communication qubits in same core
    for (int c = 0; c < device->num_cores; c++) {
        for (int j = 0; j < device->core_num_comm_qubits[c]; j++) {
            for (int k = j + 1; k < device->core_num_comm_qubits[c]; k++) {
                pqubit_t pc1 = device->core_comm_qubits[c][j];
                pqubit_t pc2 = device->core_comm_qubits[c][k];

                int distance = device_get_distance(ts->device, pc1, pc2);
                distance *= 2;

                int pc1_node = device->comm_qubit_node_id[pc1];
                int pc2_node = device->comm_qubit_node_id[pc2];

                if (pc1_node == pc2_node) continue;
                
                if (pc1 == start_qubit || pc1 == end_qubit) {
                    distance += 1;
                }
                if (pc2 == start_qubit || pc2 == end_qubit) {
                    distance += 1;
                }
                
                // Nearest Free Penalty
                float nearest_free_distance_1 = heap_get_min(layout->nearest_free_qubits[pc1_node]).priority;
                distance += nearest_free_distance_1;
                float nearest_free_distance_2 = heap_get_min(layout->nearest_free_qubits[pc2_node]).priority;
                distance += nearest_free_distance_2;

                // Full core penalty
                core_t core = device->phys_to_core[pc1];
                if (layout_get_core_remaining_capacity(layout, core) <= 2) {
                    distance += ts->config->full_core_penalty;
                }

                graph_add_edge(graph, pc1_node, pc2_node, distance);
            }
        }
    }

    // Add edges between communication qubits in different cores
    for (int e = 0; e < device->num_intercore_edges; e++) {
        pqubit_t pc1 = device->inter_core_edges[e].p1;
        pqubit_t pc2 = device->inter_core_edges[e].p2;
        int pc1_node = device->comm_qubit_node_id[pc1];
        int pc2_node = device->comm_qubit_node_id[pc2];
        int distance = 2;
        distance *= 2;
        
        // Penalty for start and end qubit in comm qubit
        if (pc1 == start_qubit || pc1 == end_qubit) {
            distance += 1;
        }
        if (pc2 == start_qubit || pc2 == end_qubit) {
            distance += 1;
        }
        
        // Full Core Penalty
        core_t core1 = device->phys_to_core[pc1];
        if (layout_get_core_remaining_capacity(layout, core1) <= 2) {
            distance += ts->config->full_core_penalty;
        }
        core_t core2 = device->phys_to_core[pc2];
        if (layout_get_core_remaining_capacity(layout, core2) <= 2) {
            distance += ts->config->full_core_penalty;
        }
        // Nearest Free Penalty
        float nearest_free_distance_1 = heap_get_min(layout->nearest_free_qubits[pc1_node]).priority;
        distance += nearest_free_distance_1;
        float nearest_free_distance_2 = heap_get_min(layout->nearest_free_qubits[pc2_node]).priority;
        distance += nearest_free_distance_2;

        graph_add_edge(graph, pc1_node, pc2_node, distance);
    }

    // Add edges from start qubit to all communication qubits in the same core
    core_t start_core = device->phys_to_core[start_qubit];
    for (int j = 0; j < device->core_num_comm_qubits[start_core]; j++) {
        pqubit_t pc = device->core_comm_qubits[start_core][j];
        int distance = abs(device_get_distance(ts->device, start_qubit, pc) - 1);
        distance *= 2;

        // Graph Node Ids
        int start_qubit_node = node_ids_out[0];
        int pc_node = device->comm_qubit_node_id[pc];

        // Nearest Free Penalty
        float nearest_free_distance = heap_get_min(layout->nearest_free_qubits[pc_node]).priority;
        distance += nearest_free_distance;

        // Full core penalty
        core_t core = device->phys_to_core[pc];
        if (layout_get_core_remaining_capacity(layout, core) <= 2) {
            distance += ts->config->full_core_penalty;
        }

        if (start_qubit_node != pc_node) {
            graph_add_directed_edge(graph, start_qubit_node, pc_node, distance);
        }
    }

    // Add edges to end qubit from all communication qubits in the same core
    core_t end_core = device->phys_to_core[end_qubit];
    for (int j = 0; j < device->core_num_comm_qubits[end_core]; j++) {
        pqubit_t pc = device->core_comm_qubits[end_core][j];
        int distance = abs(device_get_distance(ts->device, end_qubit, pc) - 1);
        distance *= 2;
        
        // Graph Node Ids
        int pc_node = device->comm_qubit_node_id[pc];
        int end_qubit_node = node_ids_out[1];

        // Nearest Free Penalty
        float nearest_free_distance = heap_get_min(layout->nearest_free_qubits[pc_node]).priority;
        distance += nearest_free_distance;

        // Full core penalty
        core_t core = device->phys_to_core[pc];
        if (layout_get_core_remaining_capacity(layout, core) <= 2) {
            distance += ts->config->full_core_penalty;
        }
        
        if (pc_node != end_qubit_node) {
            graph_add_directed_edge(graph, pc_node, end_qubit_node, distance);
        }
    }

    return graph;
}


void telesabre_reset_usage_penalties(telesabre_t* ts) {
    const device_t* device = ts->device;
    ts->usage_penalties_reset_counter--;
    if (ts->usage_penalties_reset_counter == 0) {
        for (int i = 0; i < device->num_qubits; i++) {
            ts->usage_penalties[i] = 1.0f;
        }
        ts->usage_penalties_reset_counter = ts->config->usage_penalties_reset_interval;
    }
}


void telesabre_step_free(telesabre_t* ts) {
    // Free attraction paths
    for (int i = 0; i < ts->num_attraction_paths; i++) {
        path_free(ts->attraction_paths[i]);
        ts->attraction_paths[i] = NULL;
    }
}


void telesabre_step(telesabre_t* ts) {
    const config_t* config = ts->config;
    const device_t* device = ts->device;
    const circuit_t* circuit = ts->circuit;

    layout_print(ts->layout);

    telesabre_safety_valve_check(ts);

    // Debug Print
    int num_remaining_gates = 0;
    for (int i = 0; i < circuit->num_gates; i++)
        if (ts->gate_num_remaining_parents[i] != (size_t)-1)
            num_remaining_gates++;
    printf(H1COL"\nIteration %d - Remaining Slices: %zu - Remaining Gates: %d/%zu" CRESET, 
        ts->it, ts->num_remaining_slices, num_remaining_gates, circuit->num_gates);
    if (ts->safety_valve_activated) {
        printf(" - "BHCYN"Safety Valve ON\n"CRESET);
    } else {
        printf("\n");
    }

    // Run front gates that can be run according to current layout
    bool found_executable_gate;
    ts->num_applied_gates = 0;
    do {
        found_executable_gate = false;
        // Search for runnable gates in front
        for (int i = 0; i < ts->front_size; i++) {
            const gate_t* gate = &circuit->gates[ts->front[i]];
            if (layout_can_execute_gate(ts->layout, gate)) {
                ts->applied_gates[ts->num_applied_gates++] = ts->front[i];
                telesabre_execute_front_gate(ts, i);
                telesabre_made_progress(ts);
                found_executable_gate = true;
                break;
            }
        }
    } while(found_executable_gate && ts->front_size > 0);

    // Debug Print front
    printf(H2COL"  Front size: "CRESET"%zu\n", ts->front_size);
    for (int i = 0; i < ts->front_size; i++) {
        const gate_t* gate = &circuit->gates[ts->front[i]];
        printf("    (%*zu): Virt: ", 3, ts->front[i]);
        for (int j = 0; j < gate->num_target_qubits; j++) {
            printf("%*d ", 3, gate->target_qubits[j]);
        }
        printf(" - Phys: ");
        for (int j = 0; j < gate->num_target_qubits; j++) {
            pqubit_t phys_qubit = layout_get_phys(ts->layout, gate->target_qubits[j]);
            printf("%*d ", 3, phys_qubit);
        }
        printf(" - Cores: ");
        for (int j = 0; j < gate->num_target_qubits; j++) {
            pqubit_t phys_qubit = layout_get_phys(ts->layout, gate->target_qubits[j]);
            core_t core = device->phys_to_core[phys_qubit];
            printf("%*d ", 3, core);
        }
        printf("\n");
    }

    if (ts->slices_outdated)
        telesabre_slice_remaining_circuit(ts);
    
    // Print first 3 remaining slices
    printf(H2COL"  Remaining Slices:\n"CRESET);
    for (int i = 0; i < ts->num_remaining_slices && i < 3; i++) {
        size_t slice_start = ts->remaining_slices_ptr[i];
        size_t slice_end = ts->remaining_slices_ptr[i + 1];
        printf("    Slice %d: ", i);
        for (size_t j = slice_start; j < slice_end; j++) {
            printf("%zu ", ts->remaining_slices[j]);
        }
        printf("\n");
    }

    // Search for qubit movement operations

    telesabre_calculate_attraction_paths(ts);

    telesabre_collect_traversed_comm_qubits(ts);
    telesabre_collect_nearest_free_qubits(ts);

    telesabre_collect_candidate_tele_ops(ts);
    telesabre_collect_candidate_swap_ops(ts);

    // Debug candidate op print
    printf(H2COL"  Candidate Operations:\n"CRESET);
    for (int i = 0; i < ts->num_candidate_ops; i++) {
        const op_t* op = &ts->candidate_ops[i];
        int op_qubits = op_get_num_qubits(op);
        printf("    (%*d): Type: %s, Qubits: ", 3, i, op_get_type_str(op));
        for (int j = 0; j < op_qubits; j++) {
            printf("%*d", 4, op->qubits[j]);
        }
        printf(", Front Gate Index: %d, Energy: %.3f, Flags: %s\n", op->front_gate_idx, ts->candidate_ops_energies[i], byte_to_binary(op->reasons));
        
    }

    // Find operations with lowest resulting layout energy
    int num_best_operations = 0;
    float best_energy = TS_INF;
    for (int i = 0; i < ts->num_candidate_ops; i++) {
        if (ts->candidate_ops_energies[i] < best_energy) {
            best_energy = ts->candidate_ops_energies[i];
            ts->candidate_ops[0] = ts->candidate_ops[i];
            num_best_operations = 1;
        } else if (ts->candidate_ops_energies[i] == best_energy) {
            ts->candidate_ops[num_best_operations++] = ts->candidate_ops[i];
        }
    }

    // Select a random operation from the best operations
    if (num_best_operations > 0) {
        int best_op_idx = rand() % num_best_operations;
        const op_t best_op = ts->candidate_ops[best_op_idx];
        ts->applied_op = best_op;
        telesabre_add_report_entry(ts);
        telesabre_apply_candidate_op(ts, &best_op);
    } else {
        printf("    None\n");
        ts->applied_op = (op_t){0};
        telesabre_add_report_entry(ts);
    }

    telesabre_reset_usage_penalties(ts);

    telesabre_step_free(ts);

    ts->it++;
    ts->it_without_progress++;
}


result_t telesabre_run(config_t* config, device_t* device, circuit_t* circuit) {
    srand(config->seed);
    clock_t start = clock();
    telesabre_t* ts = telesabre_init(config, device, circuit);

    // TeleSABRE Main Loop
    while (ts->front_size > 0 && ts->it < config->max_iterations) {
        telesabre_step(ts);
    }

    // Final print
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf(H1COL"\nTeleSABRE completed in %.3fs.\n" CRESET, elapsed);
    printf(H1COL"Solution has %d teledata ops, %d telegate ops and %d swaps.\n" CRESET, 
        ts->result.num_teledata, ts->result.num_telegate, ts->result.num_swaps);
    printf(H1COL"Safety Valve activated %d times.\n\n" CRESET, 
        ts->result.num_deadlocks);

    result_t result = ts->result;

    if (config->save_report) {
        report_save_as_json(
            ts->report, 
            ts->config,
            ts->device,
            ts->circuit,
            config->report_filename
        );
    }

    telesabre_free(ts);

    return result;
}


void telesabre_free(telesabre_t* ts) {
    if (!ts) return;

    free(ts->gate_num_remaining_parents);
    free(ts->front);
    layout_free(ts->layout);
    free(ts->usage_penalties);
    layout_free(ts->last_progress_layout);
    free(ts->candidate_ops);
    free(ts->candidate_ops_energies);
    free(ts->remaining_slices);
    free(ts->remaining_slices_ptr);

    free(ts->applied_gates);

    for (int i = 0; i < ts->num_attraction_paths; i++)
        if (ts->attraction_paths[i]) 
            path_free(ts->attraction_paths[i]);
    
    free(ts->attraction_paths);
    free(ts->traversed_comm_qubits);
    free(ts->nearest_free_qubits);

    report_free(ts->report);

    free(ts);

    // TODO check
}


void telesabre_add_report_entry(const telesabre_t *ts) {
    if (!ts->config->save_report) return;
    report_ensure_capacity(ts->report);

    report_entry_t entry;
    entry.it = ts->it;

    entry.num_teledata = ts->result.num_teledata;
    entry.num_telegate = ts->result.num_telegate;
    entry.num_swaps = ts->result.num_swaps;
    
    entry.safety_valve_activated = ts->safety_valve_activated;
    
    entry.phys_to_virt = malloc(sizeof(int) * ts->device->num_qubits);
    memcpy(entry.phys_to_virt, ts->layout->phys_to_virt, sizeof(int) * ts->device->num_qubits);
    
    entry.virt_to_phys = malloc(sizeof(int) * ts->device->num_qubits);
    memcpy(entry.virt_to_phys, ts->layout->virt_to_phys, sizeof(int) * ts->device->num_qubits);

    entry.remaining_gates = malloc(sizeof(int) * ts->circuit->num_gates);
    size_t num_remaining_gates = 0;
    for (int i = 0; i < ts->circuit->num_gates; i++) {
        if (ts->gate_num_remaining_parents[i] != (size_t)-1) {
            entry.remaining_gates[num_remaining_gates++] = i;
        }
    }
    entry.num_remaining_gates = num_remaining_gates;

    entry.front = malloc(sizeof(int) * ts->front_size);
    //memcpy(entry.front, ts->front, sizeof(int) * ts->front_size);
    for (int i = 0; i < ts->front_size; i++) entry.front[i] = ts->front[i];
    entry.front_size = ts->front_size;

    entry.applied_gates = malloc(sizeof(int) * ts->num_applied_gates);
    memcpy(entry.applied_gates, ts->applied_gates, sizeof(int) * ts->num_applied_gates);
    entry.num_applied_gates = ts->num_applied_gates;

    entry.applied_gates_phys = malloc(sizeof(int[GATE_MAX_TARGET_QUBITS]) * ts->num_applied_gates);
    for (int i = 0; i < ts->num_applied_gates; i++) {
        const gate_t* gate = &ts->circuit->gates[ts->applied_gates[i]];
        memset(entry.applied_gates_phys[i], -1, sizeof(int[GATE_MAX_TARGET_QUBITS]));
        for (int j = 0; j < gate->num_target_qubits; j++) {
            entry.applied_gates_phys[i][j] = layout_get_phys(ts->layout, gate->target_qubits[j]);
        }
    }

    entry.candidate_ops = malloc(sizeof(op_t) * ts->num_candidate_ops);
    memcpy(entry.candidate_ops, ts->candidate_ops, sizeof(op_t) * ts->num_candidate_ops);
    entry.candidate_ops_energies = malloc(sizeof(float) * ts->num_candidate_ops);
    memcpy(entry.candidate_ops_energies, ts->candidate_ops_energies, sizeof(float) * ts->num_candidate_ops);
    // TODO
    entry.candidate_ops_front_energies = malloc(sizeof(float) * ts->num_candidate_ops);
    memset(entry.candidate_ops_front_energies, 0, sizeof(float) * ts->num_candidate_ops);
    entry.candidate_ops_future_energies = malloc(sizeof(float) * ts->num_candidate_ops);
    memset(entry.candidate_ops_future_energies, 0, sizeof(float) * ts->num_candidate_ops);
    entry.num_candidate_ops = ts->num_candidate_ops;

    entry.attraction_paths = malloc(sizeof(path_t*) * ts->num_attraction_paths);
    for (int i = 0; i < ts->num_attraction_paths; i++) {
        entry.attraction_paths[i] = path_copy(ts->attraction_paths[i]);
    }
    entry.num_attraction_paths = ts->num_attraction_paths;

    entry.applied_op = ts->applied_op;

    // TODO
    entry.energy = 0.0f;

    ts->report->entries[ts->report->num_entries++] = entry;
}