#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

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

    float usage_penalties_reset_interval;
    bool optimize_initial;
    enum initial_layout_type initial_layout_type;

    int teleport_bonus;
    int telegate_bonus;
    int safety_valve_iters;

    int extended_set_size;
    float extended_set_factor;

    int full_core_penalty;
    int inter_core_edge_weight;
    int max_solving_deadlock_iterations;

    float gate_usage_penalty;
    float swap_usage_penalty;
    float teledata_usage_penalty;
    float telegate_usage_penalty;

    int init_layout_hun_min_free_gate;
    int init_layout_hun_min_free_qubit;

    int max_iterations;

    bool save_report;
    char report_filename[256];

    bool enable_passing_core_emptying_teleport_possibility;

    cJSON *json;
} config_t;


config_t *config_new();

config_t *config_from_json(const char *filename);

void config_free(config_t *config);