#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "circuit.h"
#include "config.h"
#include "device.h"
#include "telesabre.h"

int main(int argc, char *argv[]) {
    const char *banner = "  _____    _     ___   _   ___ ___ ___ \n"
                         " |_   _|__| |___/ __| /_\\ | _ ) _ \\ __|\n"
                         "   | |/ -_) / -_)__ \\/ _ \\| _ \\   / _| \n"
                         "   |_|\\___|_\\___|___/_/ \\_\\___/_|_\\___|\n";
    puts(banner);

    config_t *config = NULL;
    device_t *device = NULL;
    circuit_t *circuit = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *filename = argv[i];
        const char *ext = strrchr(filename, '.');
        if (strcmp(ext, ".qasm") == 0) {
            printf("Parsing .qasm file: %s\n", filename);
            circuit = circuit_from_qasm(filename);
        } else if (strcmp(ext, ".json") == 0) {
            printf("Parsing .json file: %s\n", filename);
            if (!device) device = device_from_json(filename);
            if (!config) config = config_from_json(filename);
            if (!circuit) circuit = circuit_from_json(filename);
        } else {
            fprintf(stderr, "Error: File '%s' does not have a .json or .qasm extension.\n", filename);
            return 1;
        }
    }

    if (!config)
        fprintf(stderr, "Missing config file.\n");
    if (!device)
        fprintf(stderr, "Missing device file.\n");
    if (!circuit)
        fprintf(stderr, "Missing circuit file.\n");

    if (!config || !device || !circuit) {
        fprintf(stderr, "Usage: %s <config.json> <device.json> <circuit.qasm>\n", argv[0]);
        return 1;
    }


    result_t result = {0};
    result.num_teledata = INT_MAX;

    int max_iterations = config->max_iterations;
    int successes = 0;
    for (int i = 0; i < config->max_attempts && successes < config->required_successes; i++) {
        config->max_iterations = max_iterations; // Reset max iterations for each run
        config->save_report = false;
        
        result_t result_tmp = telesabre_run(config, device, circuit);
        if (result_tmp.success) {
            printf("Telesabre run successful!\n");
            if (result_tmp.num_teledata + result_tmp.num_telegate < result.num_teledata + result.num_telegate) {
                result = result_tmp;
            }
            successes++;
        } else if (i < config->max_attempts - 1) { 
            printf("Telesabre run failed, retrying with different seed...\n");
        }
        config->seed++;
    } 

    device_print(device);

    printf("\nResult:\n");
    printf("  Depth: %d\n", result.depth);
    printf("  Teledata: %d\n", result.num_teledata);
    printf("  Telegate: %d\n", result.num_telegate);
    printf("  Swaps: %d\n", result.num_swaps);
    printf("  Deadlocks: %d\n", result.num_deadlocks);
    printf("  Success: %s\n", result.success ? "true" : "false");

    device_free(device);
    circuit_free(circuit);
    config_free(config);
    return 0;
}
