#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    
    /*
    //device_t *device = device_e();
    device_t *device = device_h();
    device_print(device);

    //circuit_t *circuit = parse_qasm_file("/Users/enrico/Documents/telesabre/qasm_hun/qnn_nativegates_ibm_qiskit_opt3_26.qasm");
    circuit_t *circuit = parse_qasm_file("/Users/enrico/Documents/telesabre/qasm_telegate/qnn_nativegates_ibm_qiskit_opt3_64.qasm");
    circuit_print(circuit);

    sliced_circuit_view_t *view = circuit_get_sliced_view(circuit, false);
    // sliced_circuit_view_print(view);

    config_t *config = new_config();
    */

    result_t result = telesabre_run(config, device, circuit);

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
