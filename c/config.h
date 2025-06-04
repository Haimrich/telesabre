#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


enum energy_type {
    ENERGY_TYPE_EXTENDED_SET,
    ENERGY_TYPE_EXPONENTIAL
};

enum initial_layout_type {
    INITIAL_LAYOUT_HUNGARIAN,
    INITIAL_LAYOUT_ROUND_ROBIN,
    INITIAL_LAYOUT_RANDOM
};

typedef struct config {
    unsigned seed;

    char name[64];
    
    enum energy_type energy_type;

    float usage_penalty;
    float usage_penalties_reset_interval;
    bool optimize_initial;
    enum initial_layout_type initial_layout_type;

    int teleport_bonus;
    int telegate_bonus;
    int safety_valve_iters;

    int extended_set_size;
    float extended_set_factor;

    int full_core_penalty;
    bool save_data;
    int max_solving_deadlock_iterations;

    float swap_decay;
    float teleport_decay;
    float telegate_decay;

    int init_layout_hun_min_free_gate;
    int init_layout_hun_min_free_qubit;

    int max_iterations;
} config_t;


config_t* new_config();

void free_config(config_t* config);