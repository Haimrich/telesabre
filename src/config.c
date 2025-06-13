#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "json.h"
#include "utils.h"


config_t* config_new() {
    config_t* config = malloc(sizeof(config_t));
    *config = (config_t){0};
    config->seed = 42;

    strcpy(config->name, "default");
    config->energy_type = ENERGY_TYPE_EXTENDED_SET;
    config->usage_penalties_reset_interval = 5;

    config->optimize_initial = false;
    config->initial_layout_type = INITIAL_LAYOUT_ROUND_ROBIN;

    config->teleport_bonus = 100;
    config->telegate_bonus = 100;

    config->safety_valve_iters = 300;

    config->extended_set_size = 20;
    config->extended_set_factor = 0.05f;

    config->full_core_penalty = 10;
    config->inter_core_edge_weight = 2;
    config->max_safety_valve_iters = 1000;

    config->gate_usage_penalty = 0;
    config->swap_usage_penalty = 0.002;
    config->teledata_usage_penalty = 0.005;
    config->telegate_usage_penalty = 0.005;

    config->init_layout_hun_min_free_gate = 4;
    config->init_layout_hun_min_free_qubit = 3;

    config->max_iterations = 1000000;
    config->max_iterations = 100; //1000000;

    config->save_report = true;
    strcpy(config->report_filename, "report.json");

    config->enable_passing_core_emptying_teleport_possibility = false;

    config->max_attempts = 10;
    config->required_successes = 1;

    config->json = NULL;
    return config;
}

config_t *config_from_json(const char* filename) {
    const char *config_json_str = read_file(filename);
    cJSON *config_json_file = cJSON_Parse(config_json_str);

    if (config_json_file == NULL) {
        fprintf(stderr, "Error parsing JSON from file %s\n", filename);
        free((void*)config_json_file);
        return NULL;
    }

    const cJSON *config_json = cJSON_GetObjectItemCaseSensitive(config_json_file, "config");
    if (config_json == NULL) {
        cJSON_Delete(config_json_file);
        free((void*)config_json_str);
        return NULL;
    }

    printf("Loading config from JSON file: %s\n", filename);
    config_t *cfg = config_new();

    #define X(name) \
        const cJSON *json_##name = cJSON_GetObjectItemCaseSensitive(config_json, #name); \
        if (json_##name) cfg->name = json_##name->valueint;
    TS_CONFIG_INT_ENTRIES
    #undef X
    
    #define X(name) \
        const cJSON *json_##name = cJSON_GetObjectItemCaseSensitive(config_json, #name); \
        if (json_##name) cfg->name = (float)json_##name->valuedouble;
    TS_CONFIG_FLOAT_ENTRIES
    #undef X

    #define X(name) \
        const cJSON *json_##name = cJSON_GetObjectItemCaseSensitive(config_json, #name); \
        if (json_##name) cfg->name = cJSON_IsTrue(json_##name);
    TS_CONFIG_BOOL_ENTRIES
    #undef X

    #define X(name) \
        const cJSON *json_##name = cJSON_GetObjectItemCaseSensitive(config_json, #name); \
        if (json_##name && cJSON_IsString(json_##name)) { \
            strncpy(cfg->name, json_##name->valuestring, sizeof(cfg->name) - 1); \
            cfg->name[sizeof(cfg->name) - 1] = '\0'; \
        }
    TS_CONFIG_STRING_ENTRIES
    #undef X

    const cJSON *json_energy_type = cJSON_GetObjectItemCaseSensitive(config_json, "energy_type");
    if (json_energy_type) {
        const char *energy_type_str = json_energy_type->valuestring;
        config_set_energy_type(cfg, energy_type_str);
    }

    const cJSON *json_initial_layout_type = cJSON_GetObjectItemCaseSensitive(config_json, "initial_layout_type");
    if (json_initial_layout_type) {
        const char *initial_layout_type_str = json_initial_layout_type->valuestring;
        config_set_initial_layout_type(cfg, initial_layout_type_str);
    }

    cfg->json = cJSON_Duplicate(config_json, 1);
    cJSON_Delete(config_json_file);
    return cfg;
}


void config_set_initial_layout_type(config_t *config, const char *value) {
    if (strcmp(value, "hungarian") == 0) {
        config->initial_layout_type = INITIAL_LAYOUT_HUNGARIAN;
    } else if (strcmp(value, "round-robin") == 0) {
        config->initial_layout_type = INITIAL_LAYOUT_ROUND_ROBIN;
    } else {
        fprintf(stderr, "Unknown initial layout type: %s\n", value);
        exit(1);
    }
}


void config_set_energy_type(config_t *config, const char *value) {
    if (strcmp(value, "extended-set") == 0) {
        config->energy_type = ENERGY_TYPE_EXTENDED_SET;
    } else if (strcmp(value, "exponential") == 0) {
        config->energy_type = ENERGY_TYPE_EXPONENTIAL;
    } else {
        fprintf(stderr, "Unknown energy type: %s\n", value);
        exit(1);
    }
}


void config_set_parameter(config_t *config, const char *key, const char *value) {
    #define X(name) \
        if (strcmp(key, #name) == 0) { \
            config->name = atoi(value); \
            return; \
        }
    TS_CONFIG_INT_ENTRIES
    #undef X

    #define X(name) \
        if (strcmp(key, #name) == 0) { \
            config->name = atof(value); \
            return; \
        }
    TS_CONFIG_FLOAT_ENTRIES
    #undef X

    #define X(name) \
        if (strcmp(key, #name) == 0) { \
            config->name = (strcmp(value, "true") == 0 || strcmp(value, "True") == 0 || strcmp(value, "1") == 0); \
            return; \
        }
    TS_CONFIG_BOOL_ENTRIES
    #undef X

    #define X(name) \
        if (strcmp(key, #name) == 0) { \
            strncpy(config->name, value, sizeof(config->name) - 1); \
            config->name[sizeof(config->name) - 1] = '\0'; \
            return; \
        }
    TS_CONFIG_STRING_ENTRIES
    #undef X

    if (strcmp(key, "initial_layout_type") == 0) {
        config_set_initial_layout_type(config, value);
        return;
    }

    if (strcmp(key, "energy_type") == 0) {
        config_set_energy_type(config, value);
        return;
    }
}


void config_free(config_t* config) {
    free(config);
}