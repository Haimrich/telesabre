#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "circuit.h"
#include "config.h"
#include "device.h"
#include "layout.h"
#include "graph.h"


typedef struct result {
    int num_teledata;
    int num_telegate;
    int num_swaps;
    int depth;
    int num_deadlocks;
} result_t;

typedef enum op_type {
    OP_SWAP,
    OP_TELEPORT,
    OP_TELEGATE,
    OP_NONE
} op_type_t;

typedef enum op_target {
    OP_SOURCE      = 0,
    OP_TARGET_A    = 0,
    OP_MEDIATOR    = 1,
    OP_MEDIATOR_A  = 1,
    OP_MEDIATOR_B  = 2,
    OP_TARGET      = 3,
    OP_TARGET_B    = 3,
} op_target_t;

typedef struct op {
    op_type_t type;
    pqubit_t qubits[4];
    int front_gate_idx;
    unsigned char reasons;
} op_t;

typedef struct {
    device_t* device;
    circuit_t* circuit;
    config_t* config;

    layout_t* layout;
    layout_t* last_progress_layout;

    float* usage_penalties;
    int usage_penalties_reset_counter;

    size_t* gate_num_remaining_parents;
    size_t* front;
    size_t front_size;
    size_t front_capacity;

    size_t *remaining_slices; // CSR
    size_t *remaining_slices_ptr;
    size_t num_remaining_slices;
    bool slices_outdated;

    op_t* candidate_ops;
    float* candidate_ops_energies;
    int num_candidate_ops;
    int candidate_ops_capacity;

    int it;
    int it_without_progress;
    bool safety_valve_activated;

    path_t** attraction_paths;
    size_t num_attraction_paths;
    size_t attraction_paths_capacity;

    pqubit_t* traversed_comm_qubits;
    size_t num_traversed_comm_qubits;
    size_t traversed_comm_qubits_capacity;

    pqubit_t* nearest_free_qubits;
    size_t num_nearest_free_qubits;
    size_t nearest_free_qubits_capacity;

    result_t result;
} telesabre_t;

result_t telesabre_run(config_t* config, device_t* device, circuit_t* circuit);

telesabre_t* telesabre_init(config_t* config, device_t* device, circuit_t* circuit);

void telesabre_step(telesabre_t* ts);
void telesabre_safety_valve_check(telesabre_t* ts);
void telesabre_execute_front_gate(telesabre_t* ts, size_t front_gate_idx);
void telesabre_made_progress(telesabre_t* ts);

void telesabre_calculate_attraction_paths(telesabre_t* ts);
void telesabre_collect_traversed_comm_qubits(telesabre_t* ts);
void telesabre_collect_nearest_free_qubits(telesabre_t* ts);

void telesabre_slice_remaining_circuit(telesabre_t* ts);

float telesabre_evaluate_op_energy(telesabre_t* ts, const op_t* op);

void telesabre_add_candidate_op(telesabre_t* ts, const op_t* op);
void telesabre_collect_candidate_tele_ops(telesabre_t* ts);
void telesabre_collect_candidate_swap_ops(telesabre_t* ts);

void telesabre_apply_candidate_op(telesabre_t* ts, const op_t* op);

void telesabre_reset_usage_penalties(telesabre_t* ts);

graph_t* telesabre_build_contracted_graph_for_pair(
    const telesabre_t* ts,
    const layout_t* layout,
    const gate_t* gate, 
    size_t node_ids_out[2],
    pqubit_t* node_id_to_phys_out
);

void telesabre_step_free(telesabre_t* ts);

void telesabre_free(telesabre_t* ts);


result_t run_telesabre(device_t* device, circuit_t* circuit, config_t* config_t);


float evaluate_op_energy(
    const config_t* config,
    const layout_t* layout, 
    const device_t* device, 
    const circuit_t* circuit, 
    const float* usage_penalties,
    const op_t* op 
);