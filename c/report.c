#include "report.h"

#include <stdio.h>

#include "config.h"
#include "device.h"
#include "circuit.h"
#include "op.h"
#include "utils.h"
#include "json.h"


report_t* report_new() {
    report_t* report = malloc(sizeof(report_t));
    *report = (report_t){0};
    report->num_entries = 0;
    report->capacity = 10;
    report->entries = malloc(sizeof(report_entry_t) * report->capacity);
    return report;
}


void report_ensure_capacity(report_t *report) {
    if (report->num_entries < report->capacity) return;
    report->capacity *= 2;
    report->entries = realloc(report->entries, sizeof(report_entry_t) * report->capacity);
}


void report_save_as_json(
    const report_t *report, 
    const config_t *config,
    const device_t *device,
    const circuit_t *circuit,
    const char *filename
) {
    cJSON *json = cJSON_CreateObject();
    cJSON *iters_json = cJSON_CreateArray();
    
    for (size_t i = 0; i < report->num_entries; i++) {
        const report_entry_t* entry = &report->entries[i];
        cJSON *entry_json = cJSON_CreateObject();

        cJSON *phys_to_virt = cJSON_CreateIntArray(entry->phys_to_virt, device->num_qubits);
        cJSON_AddItemToObject(entry_json, "phys_to_virt", phys_to_virt);
        
        cJSON *virt_to_phys = cJSON_CreateIntArray(entry->virt_to_phys, device->num_qubits);
        cJSON_AddItemToObject(entry_json, "virt_to_phys", virt_to_phys);
        
        cJSON_AddNumberToObject(entry_json, "swap_count", entry->num_swaps);
        cJSON_AddNumberToObject(entry_json, "teleportation_count", entry->num_teledata);
        cJSON_AddNumberToObject(entry_json, "telegate_count", entry->num_telegate);

        cJSON *remaining_gates = cJSON_CreateIntArray(entry->remaining_gates, entry->num_remaining_gates);
        cJSON_AddItemToObject(entry_json, "remaining_nodes", remaining_gates);

        cJSON *front = cJSON_CreateIntArray(entry->front, entry->front_size);
        cJSON_AddItemToObject(entry_json, "front", front);

        cJSON *gates = cJSON_CreateIntArray(entry->applied_gates, entry->num_applied_gates);
        cJSON_AddItemToObject(entry_json, "gates", gates);

        cJSON *applied_gates_phys = cJSON_CreateArray();
        for (size_t j = 0; j < entry->num_applied_gates; j++) {
            cJSON *gate_json = cJSON_CreateIntArray(entry->applied_gates_phys[j], GATE_MAX_TARGET_QUBITS);
            cJSON_AddItemToArray(applied_gates_phys, gate_json);
        }
        cJSON_AddItemToObject(entry_json, "applied_gates", applied_gates_phys);

        cJSON *applied_ops = cJSON_CreateArray();
        cJSON *best_op_json = cJSON_CreateIntArray(entry->applied_op.qubits, op_get_num_qubits(&entry->applied_op));
        cJSON_AddItemToArray(applied_ops, best_op_json);
        cJSON_AddItemToObject(entry_json, "applied_ops", applied_ops);

        cJSON *attraction_paths = cJSON_CreateArray();
        for (size_t j = 0; j < entry->num_attraction_paths; j++) {
            cJSON *path_json = cJSON_CreateIntArray(entry->attraction_paths[j]->nodes, entry->attraction_paths[j]->length);
            cJSON_AddItemToArray(attraction_paths, path_json);
        }
        cJSON_AddItemToObject(entry_json, "needed_paths", attraction_paths);

        cJSON_AddNumberToObject(entry_json, "energy", entry->energy);

        cJSON *candidate_ops = cJSON_CreateArray();
        for (size_t j = 0; j < entry->num_candidate_ops; j++) {
            cJSON *op_json = cJSON_CreateIntArray(entry->candidate_ops[j].qubits, op_get_num_qubits(&entry->candidate_ops[j]));
            cJSON_AddItemToArray(candidate_ops, op_json);
        }
        cJSON_AddItemToObject(entry_json, "candidate_ops", candidate_ops);

        cJSON *candidate_ops_energies = cJSON_CreateFloatArray(entry->candidate_ops_energies, entry->num_candidate_ops);
        cJSON_AddItemToObject(entry_json, "candidate_ops_scores", candidate_ops_energies);
        cJSON *candidate_ops_front_energies = cJSON_CreateFloatArray(entry->candidate_ops_front_energies, entry->num_candidate_ops);
        cJSON_AddItemToObject(entry_json, "candidate_ops_front_scores", candidate_ops_front_energies);
        cJSON *candidate_ops_future_energies = cJSON_CreateFloatArray(entry->candidate_ops_future_energies, entry->num_candidate_ops);
        cJSON_AddItemToObject(entry_json, "candidate_ops_future_scores", candidate_ops_future_energies);

        cJSON_AddBoolToObject(entry_json, "solving_deadlock", entry->safety_valve_activated);

        cJSON_AddNumberToObject(entry_json, "iteration", entry->it); 

        cJSON_AddItemToArray(iters_json, entry_json);       
    }

    cJSON_AddItemToObject(json, "iterations", iters_json);

    if (config && config->json) {
        cJSON_AddItemToObject(json, "config", cJSON_Duplicate(config->json, 1));
    }
    if (device && device->json) {
        cJSON_AddItemToObject(json, "device", cJSON_Duplicate(device->json, 1));
    }
    if (circuit && circuit->json) {
        cJSON_AddItemToObject(json, "circuit", cJSON_Duplicate(circuit->json, 1));
    }

    char *json_string = cJSON_Print(json);
    cJSON_Delete(json);

    FILE *file = fopen(filename, "w");
    if (file) {
        fputs(json_string, file);
        fclose(file);
    } else {
        error("Error saving report to %s\n", filename);
    }
}


void report_entry_free(report_entry_t* entry) {
    if (!entry) return;

    free(entry->phys_to_virt);
    free(entry->virt_to_phys);
    free(entry->remaining_gates);
    free(entry->front);
    free(entry->applied_gates);
    free(entry->candidate_ops);
    free(entry->candidate_ops_energies);
    free(entry->candidate_ops_front_energies);
    free(entry->candidate_ops_future_energies);

    for (size_t i = 0; i < entry->num_attraction_paths; i++) {
        path_free(entry->attraction_paths[i]);
    }
    free(entry->attraction_paths);
}

void report_free(report_t* report) {
    if (!report) return;

    for (size_t i = 0; i < report->num_entries; i++) {
        report_entry_free(&report->entries[i]);
    }
    free(report->entries);
    free(report);
}