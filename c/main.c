#include <stdio.h>

#include "circuit.h"
#include "config.h"
#include "device.h"
#include "telesabre.h"


int main(int argc, char *argv[]) {

    device_t *device = device_e();
    device_print(device);

    circuit_t *circuit = parse_qasm_file("/Users/enrico/Documents/telesabre/qasm_hun/qnn_nativegates_ibm_qiskit_opt3_26.qasm");
    circuit_print(circuit);

    sliced_circuit_view_t *view = circuit_get_sliced_view(circuit, false);
    // sliced_circuit_view_print(view);

    config_t *config = new_config();

    result_t result = telesabre_run(config, device, circuit);

    device_free(device);
    free_circuit(circuit);
    free_config(config);

    return 0;
}