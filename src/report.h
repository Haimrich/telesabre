#pragma once

#include <stddef.h>

#include "config.h"
#include "device.h"
#include "circuit.h"
#include "graph.h"
#include "op.h"


typedef struct report_entry {
    int it;

    int num_teledata;
    int num_telegate;
    int num_swaps;

    bool safety_valve_activated;
    int *phys_to_virt;
    int *virt_to_phys;

    int *remaining_gates;
    size_t num_remaining_gates;

    int *front;
    size_t front_size;

    int *applied_gates;
    int (*applied_gates_phys)[GATE_MAX_TARGET_QUBITS];
    size_t num_applied_gates;

    op_t *candidate_ops;
    float *candidate_ops_energies;
    float *candidate_ops_front_energies;
    float *candidate_ops_future_energies;
    size_t num_candidate_ops;

    path_t **attraction_paths;
    size_t num_attraction_paths;

    op_t applied_op;

    float energy;

} report_entry_t;


typedef struct report {
    report_entry_t *entries;
    size_t num_entries;
    size_t capacity;
} report_t;


report_t *report_new();

void report_ensure_capacity(report_t *report);

void report_entry_free(report_entry_t *entry);

void report_save_as_json(
    const report_t *report, 
    const config_t *config,
    const device_t *device,
    const circuit_t *circuit,
    const char *filename
);

void report_free(report_t *report);