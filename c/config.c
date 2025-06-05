#include "config.h"

#include <stdlib.h>
#include <stdbool.h>


config_t* new_config() {
    config_t* config = malloc(sizeof(config_t));
    *config = (config_t){0};
    config->seed = 42;

    strcpy(config->name, "default");
    config->energy_type = ENERGY_TYPE_EXTENDED_SET;
    config->usage_penalty = 0.05;
    config->usage_penalties_reset_interval = 5;

    config->optimize_initial = false;
    config->initial_layout_type = INITIAL_LAYOUT_ROUND_ROBIN;

    config->teleport_bonus = 100;
    config->telegate_bonus = 100;

    config->safety_valve_iters = 300;

    config->extended_set_size = 20;
    config->extended_set_factor = 0.05f;

    config->full_core_penalty = 10;
    config->save_data = false;
    config->max_solving_deadlock_iterations = 300;

    config->swap_decay = 0.002;
    config->teleport_decay = 0.005;
    config->telegate_decay = 0.005;

    config->init_layout_hun_min_free_gate = 4;
    config->init_layout_hun_min_free_qubit = 3;

    config->max_iterations = 1000000;
    return config;
}

void free_config(config_t* config) {
    free(config);
}