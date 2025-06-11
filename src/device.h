#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "json.h"

typedef int core_t;

typedef int pqubit_t;

typedef struct {
    pqubit_t p1;
    pqubit_t p2;
} device_edge_t;

typedef struct {
    pqubit_t p_source;
    pqubit_t p_mediator;
    pqubit_t p_target;
} device_tp_edge_t;


typedef struct {
    char name[64];

    int num_edges;
    int num_qubits;
    int num_cores;
    int core_capacity;

    // distance_matrix[core][p1][p2] = distance between p1 and p2 in core
    int*** distance_matrix;

    device_edge_t* edges;

    device_edge_t** qubit_to_edges;
    int* qubit_num_edges;

    device_edge_t* inter_core_edges;
    int num_intercore_edges;

    device_tp_edge_t* tp_edges;
    int num_tp_edges;

    pqubit_t* comm_qubits;
    int num_comm_qubits;
    bool* qubit_is_comm;
    int* comm_qubit_node_id;

    pqubit_t** core_qubits;

    pqubit_t** core_comm_qubits;
    int* core_num_comm_qubits;

    core_t* phys_to_core;

    cJSON *json;
} device_t;


device_t* device_new_grid(int core_x, int core_y, int qubit_x, int qubit_y);

device_t* device_from_json(const char *filename);

void device_update_qubit_to_edges(device_t* device);

void device_build_teleport_edges(device_t* device);

void device_calculate_distance_matrix(device_t* device);

bool device_has_edge(const device_t* device, pqubit_t p1, pqubit_t p2);

int device_get_distance(const device_t* device, pqubit_t p1, pqubit_t p2);

void device_print(const device_t* device);

void device_free(device_t* device);

device_t* device_a();
device_t* device_b();
device_t* device_c();
device_t* device_d();
device_t* device_e();
device_t* device_f();
device_t* device_g();
device_t* device_h();