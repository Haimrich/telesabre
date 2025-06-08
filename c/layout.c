#include "layout.h"

#include <stdio.h>

#include "circuit.h"
#include "device.h"
#include "heap.h"
#include "utils.h"


bool layout_is_phys_free(const layout_t *layout, pqubit_t phys) {
    return layout->phys_to_virt[phys] >= layout->circuit->num_qubits;
}

pqubit_t layout_get_phys(const layout_t *layout, vqubit_t virt) {
    return layout->virt_to_phys[virt];
}

vqubit_t layout_get_virt(const layout_t *layout, pqubit_t phys) {
    return layout->phys_to_virt[phys];
}

core_t layout_get_virt_core(const layout_t *layout, vqubit_t virt) {
    return layout->device->phys_to_core[layout->virt_to_phys[virt]];
}

int layout_get_core_remaining_capacity(const layout_t *layout, core_t core) {
    return layout->core_remaining_capacities[core];
}

bool layout_can_execute_gate(const layout_t *layout, const gate_t *gate) {
    if (gate->num_target_qubits < 2) {
        return true;
    }

    vqubit_t virt1 = gate->target_qubits[0];
    vqubit_t virt2 = gate->target_qubits[1];
    pqubit_t phys1 = layout->virt_to_phys[virt1];
    pqubit_t phys2 = layout->virt_to_phys[virt2];
    core_t core1 = layout->device->phys_to_core[phys1];
    core_t core2 = layout->device->phys_to_core[phys2];
    if (core1 != core2) return false;

    pqubit_t p_offset = layout->device->core_qubits[core1][0];
    return layout->device->distance_matrix[core1][phys1 - p_offset][phys2 - p_offset] == 1;
}

bool layout_gate_is_separated(const layout_t *layout, const gate_t *gate) {
    if (gate->num_target_qubits < 2) {
        return false;
    }

    vqubit_t virt1 = gate->target_qubits[0];
    vqubit_t virt2 = gate->target_qubits[1];
    pqubit_t phys1 = layout->virt_to_phys[virt1];
    pqubit_t phys2 = layout->virt_to_phys[virt2];
    core_t core1 = layout->device->phys_to_core[phys1];
    core_t core2 = layout->device->phys_to_core[phys2];

    return core1 != core2;
}

void layout_apply_swap(layout_t *layout, pqubit_t phys1, pqubit_t phys2) {
    if (phys1 == phys2) {
        error("Cannot swap the same physical qubit %d.", phys1);
    }

    if (layout_is_phys_free(layout, phys1) && layout_is_phys_free(layout, phys2)) {
        error("Cannot swap physical qubits %d and %d: both are free.", phys1, phys2);
    }


    int virt1 = layout->phys_to_virt[phys1];
    int virt2 = layout->phys_to_virt[phys2];
    layout->phys_to_virt[phys1] = virt2;
    layout->phys_to_virt[phys2] = virt1;
    layout->virt_to_phys[virt1] = phys2;
    layout->virt_to_phys[virt2] = phys1;

    core_t core = layout->device->phys_to_core[phys1];
    pqubit_t p_offset = layout->device->core_qubits[core][0];

    for (int i = 0; i < layout->device->core_num_comm_qubits[core]; i++) {
        pqubit_t comm_qubit = layout->device->core_comm_qubits[core][i];
        int pc_id = layout->device->comm_qubit_node_id[comm_qubit];

        if (layout_is_phys_free(layout, phys1)) {
            int new_distance_p1 = device_get_distance(layout->device, comm_qubit, phys1);
            heap_insert(layout->nearest_free_qubits[pc_id], phys1 - p_offset, new_distance_p1);
            heap_remove(layout->nearest_free_qubits[pc_id], phys2 - p_offset);
        } else if (layout_is_phys_free(layout, phys2)) {
            int new_distance_p2 = device_get_distance(layout->device, comm_qubit, phys2);
            heap_insert(layout->nearest_free_qubits[pc_id], phys2 - p_offset, new_distance_p2);
            heap_remove(layout->nearest_free_qubits[pc_id], phys1 - p_offset);
        }
    }
}

void layout_apply_teleport(layout_t *layout, pqubit_t phys_source, pqubit_t phys_mediator, pqubit_t phys_target) {
    if (layout_is_phys_free(layout, phys_source)) {
        error("Cannot teleport with empty source physical qubit %d.", phys_source);
    } else if (!layout_is_phys_free(layout, phys_mediator)) {
        error("Cannot teleport with non-free mediator physical qubit %d.", phys_mediator);
    } else if (!layout_is_phys_free(layout, phys_target)) {
        error("Cannot teleport to non-empty target physical qubit %d.", phys_target);
    }

    int virt_src = layout->phys_to_virt[phys_source];
    int virt_tgt = layout->phys_to_virt[phys_target];
    layout->phys_to_virt[phys_source] = virt_tgt;
    layout->phys_to_virt[phys_target] = virt_src;
    layout->virt_to_phys[virt_src] = phys_target;
    layout->virt_to_phys[virt_tgt] = phys_source;

    core_t core_source = layout->device->phys_to_core[phys_source];
    core_t core_target = layout->device->phys_to_core[phys_target];

    layout->core_remaining_capacities[core_source] += 1;
    layout->core_remaining_capacities[core_target] -= 1;

    pqubit_t p_offset_source = layout->device->core_qubits[core_source][0];
    pqubit_t p_offset_target = layout->device->core_qubits[core_target][0];

    // Remove free qubit from core_target nearest qubits
    for (int i = 0; i < layout->device->core_num_comm_qubits[core_target]; i++) {
        pqubit_t comm_qubit = layout->device->core_comm_qubits[core_target][i];
        int pc_id = layout->device->comm_qubit_node_id[comm_qubit];
        heap_remove(layout->nearest_free_qubits[pc_id], phys_target - p_offset_target);
    }

    // Add free qubit to core_source nearest qubits
    for (int i = 0; i < layout->device->core_num_comm_qubits[core_source]; i++) {
        pqubit_t comm_qubit = layout->device->core_comm_qubits[core_source][i];
        int pc_id = layout->device->comm_qubit_node_id[comm_qubit];
        heap_insert(layout->nearest_free_qubits[pc_id], phys_source - p_offset_source, device_get_distance(layout->device, comm_qubit, phys_source));
    }
}

pqubit_t layout_get_nearest_free_qubit(const layout_t *layout, int comm_qubit_id) {
    const device_t *dev = layout->device;
    core_t core = dev->phys_to_core[dev->comm_qubits[comm_qubit_id]];
    heap_t *heap = layout->nearest_free_qubits[comm_qubit_id];

    if (heap_is_empty(heap)) {
        printf("No free qubits available for communication qubit %d\n", comm_qubit_id);
        exit(1);
        return -1;  // No free qubits available
    }

    int p_free = heap_get_min(heap).id;
    pqubit_t p_offset = dev->core_qubits[core][0];
    return p_free + p_offset;
}

void layout_init_nearest_free_qubits(layout_t *layout) {
    const device_t *dev = layout->device;
    layout->nearest_free_qubits = malloc(sizeof(heap_t *) * dev->num_comm_qubits);
    for (int i = 0; i < dev->num_comm_qubits; i++) {
        heap_t *heap = heap_new(dev->core_capacity);
        pqubit_t p_comm = dev->comm_qubits[i];
        core_t p_comm_core = dev->phys_to_core[p_comm];
        pqubit_t p_offset = dev->core_qubits[p_comm_core][0];  // offset for this core, assuming qubits are contiguous and ordered in each core
        for (int j = 0; j < dev->core_capacity; j++) {
            pqubit_t p = dev->core_qubits[p_comm_core][j];
            if (layout_is_phys_free(layout, p)) {
                int distance = device_get_distance(dev, p_comm, p);
                printf("distance from comm qubit %d to physical qubit %d: %d\n", p_comm, p, distance);
                heap_insert(heap, p - p_offset, distance);
            }
        }
        layout->nearest_free_qubits[i] = heap;
        printf("nearest free qubits for comm qubit %d: %d\n", i, heap_get_min(heap).id);
    }
}

layout_t *layout_new(const device_t *device, const circuit_t *circuit) {
    layout_t *layout = malloc(sizeof(layout_t));
    layout->phys_to_virt = malloc(sizeof(vqubit_t) * device->num_qubits);
    layout->virt_to_phys = malloc(sizeof(pqubit_t) * device->num_qubits);
    layout->core_remaining_capacities = malloc(sizeof(int) * device->num_cores);

    for (pqubit_t p = 0; p < device->num_qubits; p++) {
        layout->phys_to_virt[p] = -1;
        layout->virt_to_phys[p] = -1;
    }
    for (core_t c = 0; c < device->num_cores; c++) {
        layout->core_remaining_capacities[c] = device->core_capacity;
    }

    layout->nearest_free_qubits = NULL;

    layout->device = device;
    layout->circuit = circuit;

    return layout;
}

layout_t *layout_copy(const layout_t *layout) {
    const device_t *device = layout->device;

    layout_t *new_layout = malloc(sizeof(layout_t));
    new_layout->phys_to_virt = malloc(sizeof(vqubit_t) * device->num_qubits);
    new_layout->virt_to_phys = malloc(sizeof(pqubit_t) * device->num_qubits);
    new_layout->core_remaining_capacities = malloc(sizeof(int) * device->num_cores);

    memcpy(new_layout->phys_to_virt, layout->phys_to_virt, sizeof(vqubit_t) * device->num_qubits);
    memcpy(new_layout->virt_to_phys, layout->virt_to_phys, sizeof(pqubit_t) * device->num_qubits);
    memcpy(new_layout->core_remaining_capacities, layout->core_remaining_capacities, sizeof(int) * device->num_cores);

    if (layout->nearest_free_qubits != NULL) {
        new_layout->nearest_free_qubits = malloc(sizeof(heap_t *) * device->num_comm_qubits);
        for (int i = 0; i < device->num_comm_qubits; i++) 
            new_layout->nearest_free_qubits[i] = heap_copy(layout->nearest_free_qubits[i]);
    } else {
        new_layout->nearest_free_qubits = NULL;
    }

    new_layout->device = layout->device;
    new_layout->circuit = layout->circuit;

    return new_layout;
}

void layout_free(layout_t *layout) {
    free(layout->phys_to_virt);
    free(layout->virt_to_phys);
    free(layout->core_remaining_capacities);

    if (layout->nearest_free_qubits != NULL) {
        for (int i = 0; i < layout->device->num_comm_qubits; i++) 
            heap_free(layout->nearest_free_qubits[i]);
        free(layout->nearest_free_qubits);
    }

    free(layout);
}

void layout_print(const layout_t *layout) {
    printf(BRED "\nLayout:\n" CRESET);
    printf(HRED "  Phys-to-virt:" CRESET);
    for (pqubit_t p = 0; p < layout->device->num_qubits; p++) {
        const char *color = (layout->device->qubit_is_comm[p]) ? MAG : RED;
        if (p % 8 == 0) {
            printf("\n  ");
        }
        if (layout->phys_to_virt[p] >= layout->circuit->num_qubits) {
            printf("%s%*d("CRESET"%*s%s)"CRESET, color, 4, p, 3, " ", color);
        } else {
            printf("%s%*d("CRESET"%*d%s)"CRESET, color, 4, p, 3, layout->phys_to_virt[p], color);
        }
        
    }
    printf(HRED"\n  Virt-to-phys:"CRESET);
    for (vqubit_t v = 0; v < layout->circuit->num_qubits; v++) {
        if (v % 8 == 0) {
            printf("\n  ");
        }
        printf(RED"%*d→"CRESET"%*d ", 4, v, 3, layout->virt_to_phys[v]);
    }
    printf(HRED"\n  Core curr. capacities:"CRESET);
    for (core_t c = 0; c < layout->device->num_cores; c++) {
        if (c % 8 == 0) {
            printf("\n  ");
        }
        printf(RED"%*d["CRESET"%*d"RED"]"CRESET, 4, c, 3, layout->core_remaining_capacities[c]);
        
    }
    printf(HRED"\n  Nearest free qubits:"CRESET);
    if (layout->nearest_free_qubits == NULL) {
        printf("None\n");
    } else {
        for (int i = 0; i < layout->device->num_comm_qubits; i++) {
            if (i % 8 == 0) {
                printf("\n  ");
            }
            int nearest_id = heap_get_min(layout->nearest_free_qubits[i]).id;
            core_t core = layout->device->phys_to_core[layout->device->comm_qubits[i]];
            pqubit_t p_offset = layout->device->core_qubits[core][0];
            if (nearest_id == -1) {
                printf(MAG"%*d→"CRESET"%*s ", 4, layout->device->comm_qubits[i], 3, " ");
            } else {
                printf(MAG"%*d→"CRESET"%*d ", 4, layout->device->comm_qubits[i], 3, nearest_id + p_offset);
            }
        }
    }

    printf("\n");
}


layout_t *initial_layout(device_t *device, circuit_t *circuit, config_t *config) {
    layout_t *layout = NULL;

    switch (config->initial_layout_type) {
        case INITIAL_LAYOUT_HUNGARIAN:
            layout = initial_layout_hungarian(device, circuit, config);
            break;
        case INITIAL_LAYOUT_ROUND_ROBIN:
            layout = initial_layout_round_robin(device, circuit, config);
            break;
        default:
            layout = initial_layout_random(device, circuit, config);
    }

    layout_init_nearest_free_qubits(layout);
    return layout;
}


layout_t *initial_layout_hungarian(device_t *device, circuit_t *circuit, config_t *config) {
    // Initialize layout
    layout_t *layout = layout_new(device, circuit);

    core_t *virt_to_core = malloc(sizeof(core_t) * device->num_qubits);
    for (vqubit_t q = 0; q < device->num_qubits; q++) virt_to_core[q] = -1;

    size_t *core_capacities = malloc(sizeof(size_t) * device->num_cores);
    for (int c = 0; c < device->num_cores; c++) core_capacities[c] = device->core_capacity;

    // Consider first circuit slice of two-qubit gates
    sliced_circuit_view_t *sliced_view = circuit_get_sliced_view(circuit, true);
    size_t *front_gates = sliced_view->slices[0];
    size_t front_size = sliced_view->slice_sizes[0];

    // Assign as many interacting qubits as possible to the same core
    for (size_t g = 0; g < front_size; g++) {
        for (core_t c = 0; c < device->num_cores; c++) {
            if (core_capacities[c] > config->init_layout_hun_min_free_gate) {
                gate_t *gate = &circuit->gates[front_gates[g]];
                if (gate->num_target_qubits < 2) continue;
                vqubit_t virt1 = gate->target_qubits[0];
                vqubit_t virt2 = gate->target_qubits[1];
                virt_to_core[virt1] = c;
                virt_to_core[virt2] = c;
                core_capacities[c] -= 2;
                break;
            }
        }
    }
    // Assign remaining qubits to cores
    for (vqubit_t q = 0; q < circuit->num_qubits; q++) {
        if (virt_to_core[q] == -1) {
            for (core_t c = 0; c < device->num_cores; c++) {
                if (core_capacities[c] > config->init_layout_hun_min_free_qubit) {
                    virt_to_core[q] = c;
                    core_capacities[c] -= 1;
                    break;
                }
            }
        }
    }
    // Now assign vqubit to specific pqubit. Create list of qubits in each core.
    vqubit_t **core_to_virt = malloc(sizeof(vqubit_t *) * device->num_cores);
    size_t *qubits_in_core = malloc(sizeof(size_t) * device->num_cores);
    memset(qubits_in_core, 0, sizeof(size_t) * device->num_cores);
    for (core_t c = 0; c < device->num_cores; c++) {
        core_to_virt[c] = malloc(sizeof(vqubit_t) * device->core_capacity);
        memset(core_to_virt[c], -1, sizeof(vqubit_t) * device->core_capacity);
    }
    for (vqubit_t q = 0; q < circuit->num_qubits; q++) {
        core_t c = virt_to_core[q];
        core_to_virt[c][qubits_in_core[c]] = q;
        qubits_in_core[c]++;
    }
    // Random permutation of qubits in each core
    pqubit_t *permutation = malloc(sizeof(pqubit_t) * device->num_qubits);
    for (pqubit_t p = 0; p < device->num_qubits; p++) permutation[p] = p;
    fisher_yates(permutation, device->num_qubits, sizeof(pqubit_t));
    // Assign virtual qubits to physical qubits
    vqubit_t virt_empty = circuit->num_qubits;
    for (pqubit_t p = 0; p < device->num_qubits; p++) {
        pqubit_t pp = permutation[p];
        core_t c = device->phys_to_core[pp];
        if (qubits_in_core[c] > 0) {
            layout->phys_to_virt[pp] = core_to_virt[c][qubits_in_core[c] - 1];
            layout->virt_to_phys[core_to_virt[c][qubits_in_core[c] - 1]] = pp;
            qubits_in_core[c]--;
            layout->core_remaining_capacities[c]--;
        } else {
            layout->phys_to_virt[pp] = virt_empty;
            layout->virt_to_phys[virt_empty] = pp;
            virt_empty++;
        }
    }

    // Cleanup
    sliced_circuit_view_free(sliced_view);
    free(virt_to_core);
    free(core_capacities);
    for (core_t c = 0; c < device->num_cores; c++) free(core_to_virt[c]);
    free(core_to_virt);
    free(qubits_in_core);
    free(permutation);

    return layout;
}


layout_t *initial_layout_round_robin(device_t *device, circuit_t *circuit, config_t *config) {
    // Initialize layout
    layout_t *layout = layout_new(device, circuit);
    // Assign virtual qubits to cores in round-robin fashion
    core_t *virt_to_core = malloc(sizeof(core_t) * device->num_qubits);
    for (vqubit_t q = 0; q < circuit->num_qubits; q++) {
        virt_to_core[q] = q % device->num_cores;
    }
    // Create list of qubits in each core
    vqubit_t **core_to_virt = malloc(sizeof(vqubit_t *) * device->num_cores);
    size_t *qubits_in_core = malloc(sizeof(size_t) * device->num_cores);
    memset(qubits_in_core, 0, sizeof(size_t) * device->num_cores);
    for (core_t c = 0; c < device->num_cores; c++) {
        core_to_virt[c] = malloc(sizeof(vqubit_t) * device->core_capacity);
        memset(core_to_virt[c], -1, sizeof(vqubit_t) * device->core_capacity);
    }
    for (vqubit_t q = 0; q < circuit->num_qubits; q++) {
        core_t c = virt_to_core[q];
        core_to_virt[c][qubits_in_core[c]] = q;
        qubits_in_core[c]++;
    }
    // Random permutation of qubits in each core
    pqubit_t *permutation = malloc(sizeof(pqubit_t) * device->num_qubits);
    for (pqubit_t p = 0; p < device->num_qubits; p++) permutation[p] = p;
    fisher_yates(permutation, device->num_qubits, sizeof(pqubit_t));
    // Assign virtual qubits to physical qubits
    vqubit_t virt_empty = circuit->num_qubits;
    for (pqubit_t p = 0; p < device->num_qubits; p++) {
        pqubit_t pp = permutation[p];
        core_t c = device->phys_to_core[pp];
        if (qubits_in_core[c] > 0) {
            layout->phys_to_virt[pp] = core_to_virt[c][qubits_in_core[c] - 1];
            layout->virt_to_phys[core_to_virt[c][qubits_in_core[c] - 1]] = pp;
            qubits_in_core[c]--;
            layout->core_remaining_capacities[c]--;
        } else {
            layout->phys_to_virt[pp] = virt_empty;
            layout->virt_to_phys[virt_empty] = pp;
            virt_empty++;
        }
    }
    // Cleanup
    free(virt_to_core);
    for (core_t c = 0; c < device->num_cores; c++) free(core_to_virt[c]);
    free(core_to_virt);
    free(qubits_in_core);
    free(permutation);

    return layout;
}


layout_t *initial_layout_random(device_t *device, circuit_t *circuit, config_t *config) {
    // Initialize layout
    layout_t *layout = layout_new(device, circuit);
    // Assign virtual qubits to phyisical qubits randomly
    pqubit_t *permutation = malloc(sizeof(pqubit_t) * device->num_qubits);
    for (pqubit_t p = 0; p < device->num_qubits; p++) permutation[p] = p;
    fisher_yates(permutation, device->num_qubits, sizeof(pqubit_t));
    // Assign virtual qubits to physical qubits
    vqubit_t virt_empty = circuit->num_qubits;
    vqubit_t virt = 0;
    for (pqubit_t p = 0; p < device->num_qubits; p++) {
        pqubit_t pp = permutation[p];
        core_t c = device->phys_to_core[pp];
        if (layout->core_remaining_capacities[c] > 1 && virt < circuit->num_qubits) {
            layout->core_remaining_capacities[c]--;
            layout->phys_to_virt[pp] = virt;
            layout->virt_to_phys[virt] = pp;
            virt++;
        } else {
            layout->phys_to_virt[pp] = virt_empty;
            layout->virt_to_phys[virt_empty] = pp;
            virt_empty++;
        }
    }

    return layout;
}
