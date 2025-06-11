
#pragma once


#include "circuit.h"
#include "config.h"
#include "device.h"
#include "heap.h"


typedef struct {
    vqubit_t *phys_to_virt;          // Maps physical qubits to virtual qubits
    pqubit_t *virt_to_phys;          // Maps virtual qubits to physical qubits
    int *core_remaining_capacities;  // Number of free physical positions in each core
    heap_t **nearest_free_qubits;    // Nearest free qubit min heap for each communication qubit

    const device_t *device;    // Pointer to the device this layout is for
    const circuit_t *circuit;  // Pointer to the circuit this layout is for
} layout_t;

bool layout_is_phys_free(const layout_t *layout, pqubit_t phys);

pqubit_t layout_get_phys(const layout_t *layout, vqubit_t virt);

vqubit_t layout_get_virt(const layout_t *layout, pqubit_t phys);

core_t layout_get_virt_core(const layout_t *layout, vqubit_t virt);

int layout_get_core_remaining_capacity(const layout_t *layout, core_t core);

bool layout_can_execute_gate(const layout_t *layout, const gate_t *gate);

bool layout_gate_is_separated(const layout_t *layout, const gate_t *gate);

void layout_apply_swap(layout_t *layout, pqubit_t phys1, pqubit_t phys2);

void layout_apply_teleport(layout_t *layout, pqubit_t phys_source, pqubit_t phys_mediator, pqubit_t phys_target);

pqubit_t layout_get_nearest_free_qubit(const layout_t *layout, pqubit_t qubit);

void layout_init_nearest_free_qubits(layout_t *layout);

layout_t *layout_new(const device_t *device, const circuit_t *circuit);

layout_t *layout_copy(const layout_t *layout);

void layout_free(layout_t *layout);

void layout_print(const layout_t *layout);


layout_t *initial_layout(device_t *device, circuit_t *circuit, config_t *config);
layout_t *initial_layout_hungarian(device_t *device, circuit_t *circuit, config_t *config);
layout_t *initial_layout_round_robin(device_t *device, circuit_t *circuit, config_t *config);
layout_t *initial_layout_random(device_t *device, circuit_t *circuit, config_t *config);