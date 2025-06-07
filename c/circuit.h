#pragma once

#include <stdlib.h>
#include <stdbool.h>

#define GATE_MAX_TARGET_QUBITS 2
#define GATE_MAX_TYPE_LENGTH 8
#define QASM_MAX_LINE_LENGTH 256

typedef int vqubit_t;

typedef struct gate
{
    size_t id;
    char type[GATE_MAX_TYPE_LENGTH];

    vqubit_t target_qubits[GATE_MAX_TARGET_QUBITS];
    size_t num_target_qubits;
    
    size_t children_id[GATE_MAX_TARGET_QUBITS];
    size_t num_children;
    size_t num_parents;
} gate_t;


typedef struct circuit
{
    size_t num_qubits;

    gate_t *gates;
    size_t num_gates;
} circuit_t;


typedef struct sliced_circuit_view
{
    circuit_t *circuit;
    size_t num_slices;
    size_t *slice_sizes;
    size_t **slices;        // gate_ids
    size_t *gate_slices;    // slice_id for each gate
} sliced_circuit_view_t;



bool gate_is_two_qubit(gate_t *gate);

bool gates_share_qubits(gate_t *gate1, gate_t *gate2);


circuit_t* parse_qasm_file(const char *filename);

void circuit_build_dependencies(circuit_t *circuit);

void circuit_print(circuit_t *circuit);

void free_circuit(circuit_t *circuit);


sliced_circuit_view_t* circuit_get_sliced_view(circuit_t *circuit, bool two_qubit_only);

void sliced_circuit_view_print(sliced_circuit_view_t *view);

void free_sliced_circuit_view(sliced_circuit_view_t *view);