#include "device.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "utils.h"


device_t* device_new_grid(int core_x, int core_y, int qubit_x, int qubit_y) {
    device_t* dev = malloc(sizeof(device_t));
    *dev = (device_t){0};

    dev->num_qubits = core_x * core_y * qubit_x * qubit_y;
    dev->num_cores = core_x * core_y;
    dev->core_capacity = qubit_x * qubit_y;

    dev->phys_to_core = malloc(sizeof(core_t) * dev->num_qubits);
    dev->core_qubits = malloc(sizeof(pqubit_t*) * dev->num_cores);
    for (core_t c = 0; c < dev->num_cores; c++) dev->core_qubits[c] = malloc(sizeof(pqubit_t) * dev->core_capacity);

    dev->num_intercore_edges = core_x * (core_y - 1) + core_y * (core_x - 1);
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);
    dev->num_edges = dev->num_cores * (qubit_y * (qubit_x - 1) + qubit_x * (qubit_y - 1));
    dev->edges = malloc(sizeof(device_edge_t) * dev->num_edges);

    int ice = 0;
    int iqe = 0;
    for (core_t cy = 0; cy < core_y; cy++) {
        for (core_t cx = 0; cx < core_x; cx++) {
            pqubit_t core_first_pqubit = (cy * core_x + cx) * qubit_x * qubit_y;
            core_t core_id = cy * core_x + cx;

            // teleport edges
            if (cx < core_x - 1) {  // Horizontal teleport connection
                dev->inter_core_edges[ice++] = (device_edge_t){.p1 = core_first_pqubit, .p2 = core_first_pqubit + qubit_x * qubit_y};
            }
            if (cy < core_y - 1) {  // Vertical teleport connection
                dev->inter_core_edges[ice++] = (device_edge_t){.p1 = core_first_pqubit, .p2 = core_first_pqubit + qubit_x * qubit_y * core_x};
            }

            for (pqubit_t y = 0; y < qubit_y; y++) {
                for (pqubit_t x = 0; x < qubit_x; x++) {
                    pqubit_t node_index = core_first_pqubit + y * qubit_x + x;
                    dev->phys_to_core[node_index] = cy * core_x + cx;
                    dev->core_qubits[core_id][y * qubit_x + x] = node_index;

                    // Intra-core grid edges
                    if (x < qubit_x - 1) {  // Connect to right neighbor within the core
                        dev->edges[iqe++] = (device_edge_t){.p1 = node_index, .p2 = node_index + 1};
                    }
                    if (y < qubit_y - 1) {  // Connect to bottom neighbor within the core
                        dev->edges[iqe++] = (device_edge_t){.p1 = node_index, .p2 = node_index + qubit_x};
                    }
                }
            }
        }
    }

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    device_calculate_distance_matrix(dev);

    dev->json = NULL;
    return dev;
}

device_t *device_from_json(const char *filename) {
    const char *device_json_str = read_file(filename);
    cJSON *device_json_file = cJSON_Parse(device_json_str);

    if (device_json_file == NULL) {
        fprintf(stderr, "Error parsing JSON from file %s\n", filename);
        free((void*)device_json_str);
        return NULL;
    }

    const cJSON *device_json = cJSON_GetObjectItemCaseSensitive(device_json_file, "device");
    if (device_json == NULL) {
        cJSON_Delete(device_json_file);
        free((void*)device_json_str);
        return NULL;
    }

    printf("Loading device from JSON file: %s\n", filename);

    device_t *dev = malloc(sizeof(device_t));
    *dev = (device_t){0};

    strncpy(dev->name, cJSON_GetObjectItemCaseSensitive(device_json, "name")->valuestring, sizeof(dev->name) - 1);
    dev->num_qubits = cJSON_GetObjectItemCaseSensitive(device_json, "num_qubits")->valueint;
    dev->num_cores = cJSON_GetObjectItemCaseSensitive(device_json, "num_cores")->valueint;
    dev->core_capacity = dev->num_qubits / dev->num_cores;

    dev->phys_to_core = malloc(sizeof(core_t) * dev->num_qubits);
    for (pqubit_t i = 0; i < dev->num_qubits; i++) {
        dev->phys_to_core[i] = i / dev->core_capacity;
    }

    dev->core_qubits = malloc(sizeof(pqubit_t*) * dev->num_cores);
    for (core_t c = 0; c < dev->num_cores; c++) {
        dev->core_qubits[c] = malloc(sizeof(pqubit_t) * dev->core_capacity);
        for (pqubit_t i = 0; i < dev->core_capacity; i++) {
            dev->core_qubits[c][i] = c * dev->core_capacity + i;
        }
    }

    const cJSON *edge_json = NULL;

    const cJSON *inter_core_edges_json = cJSON_GetObjectItemCaseSensitive(device_json, "inter_core_edges");
    dev->num_intercore_edges = 0;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * cJSON_GetArraySize(inter_core_edges_json));
    cJSON_ArrayForEach(edge_json, inter_core_edges_json) {
        dev->inter_core_edges[dev->num_intercore_edges++] = (device_edge_t){
            .p1 = cJSON_GetArrayItem(edge_json, 0)->valueint,
            .p2 = cJSON_GetArrayItem(edge_json, 1)->valueint
        };
    }

    const cJSON *edges_json = cJSON_GetObjectItemCaseSensitive(device_json, "intra_core_edges");
    dev->num_edges = 0;
    dev->edges = malloc(sizeof(device_edge_t) * cJSON_GetArraySize(edges_json));
    cJSON_ArrayForEach(edge_json, edges_json) {
        dev->edges[dev->num_edges++] = (device_edge_t){
            .p1 = cJSON_GetArrayItem(edge_json, 0)->valueint,
            .p2 = cJSON_GetArrayItem(edge_json, 1)->valueint
        };
    }

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    device_calculate_distance_matrix(dev);
    
    dev->json = cJSON_Duplicate(device_json, true);
    cJSON_Delete(device_json_file);
    return dev;
}


void device_update_qubit_to_edges(device_t* dev) {
    if (dev->qubit_to_edges != NULL) {
        for (pqubit_t i = 0; i < dev->num_qubits; i++)
            if (dev->qubit_to_edges[i] != NULL) free(dev->qubit_to_edges[i]);
        free(dev->qubit_to_edges);
    }

    if (dev->qubit_num_edges != NULL) free(dev->qubit_num_edges);
    dev->qubit_num_edges = calloc(dev->num_qubits, sizeof(int));

    dev->qubit_to_edges = malloc(sizeof(device_edge_t*) * dev->num_qubits);
    for (pqubit_t i = 0; i < dev->num_qubits; i++) {
        for (int j = 0; j < dev->num_edges; j++)
            if (dev->edges[j].p1 == i || dev->edges[j].p2 == i) dev->qubit_num_edges[i]++;

        dev->qubit_to_edges[i] = malloc(sizeof(device_edge_t) * dev->qubit_num_edges[i]);
        int k = 0;
        for (int j = 0; j < dev->num_edges; j++) {
            if (dev->edges[j].p1 == i || dev->edges[j].p2 == i) {
                dev->qubit_to_edges[i][k] = dev->edges[j];
                k++;
            }
        }
    }
}


void device_build_teleport_edges(device_t* dev) {
    if (dev->tp_edges != NULL) free(dev->tp_edges);
    dev->num_tp_edges = 0;
    dev->tp_edges = NULL;

    bool* qubit_is_comm = calloc(dev->num_qubits, sizeof(bool));

    for (int e = 0; e < dev->num_intercore_edges; e++) {
        pqubit_t p1 = dev->inter_core_edges[e].p1;
        pqubit_t p2 = dev->inter_core_edges[e].p2;

        // Mark the qubits as communication qubits
        qubit_is_comm[p1] = true;
        qubit_is_comm[p2] = true;

        // Forward direction
        for (int e_ = 0; e_ < dev->qubit_num_edges[p1]; e_++) {
            pqubit_t p1_neighbor = dev->qubit_to_edges[p1][e_].p1;
            if (p1_neighbor == p1) {
                p1_neighbor = dev->qubit_to_edges[p1][e_].p2;
            }
            dev->tp_edges = realloc(dev->tp_edges, sizeof(device_tp_edge_t) * (dev->num_tp_edges + 1));
            dev->tp_edges[dev->num_tp_edges++] = (device_tp_edge_t){.p_source = p1_neighbor, .p_mediator = p1, .p_target = p2};
        }

        // Reverse direction
        for (int e_ = 0; e_ < dev->qubit_num_edges[p2]; e_++) {
            pqubit_t p2_neighbor = dev->qubit_to_edges[p2][e_].p1;
            if (p2_neighbor == p2) {
                p2_neighbor = dev->qubit_to_edges[p2][e_].p2;
            }
            dev->tp_edges = realloc(dev->tp_edges, sizeof(device_tp_edge_t) * (dev->num_tp_edges + 1));
            dev->tp_edges[dev->num_tp_edges++] = (device_tp_edge_t){.p_source = p2_neighbor, .p_mediator = p2, .p_target = p1};
        }
    }

    // Initialize comm qubit list

    if (dev->comm_qubits != NULL) free(dev->comm_qubits);
    dev->num_comm_qubits = 0;
    dev->comm_qubits = NULL;
    dev->qubit_is_comm = qubit_is_comm;

    if (dev->comm_qubit_node_id != NULL)
        free(dev->comm_qubit_node_id);
    dev->comm_qubit_node_id = malloc(sizeof(int) * dev->num_qubits);
    for (pqubit_t i = 0; i < dev->num_qubits; i++)
        dev->comm_qubit_node_id[i] = -1;

    if (dev->core_comm_qubits != NULL) {
        for (core_t i = 0; i < dev->num_cores; i++) {
            if (dev->core_comm_qubits[i] != NULL) free(dev->core_comm_qubits[i]);
            dev->core_comm_qubits[i] = NULL;
            dev->core_num_comm_qubits[i] = 0;
        }
        free(dev->core_comm_qubits);
        free(dev->core_num_comm_qubits);
    }
    dev->core_comm_qubits = calloc(dev->num_cores, sizeof(pqubit_t*));
    dev->core_num_comm_qubits = calloc(dev->num_cores, sizeof(int));

    for (pqubit_t i = 0; i < dev->num_qubits; i++) {
        if (qubit_is_comm[i]) {
            dev->comm_qubits = realloc(dev->comm_qubits, sizeof(pqubit_t) * (dev->num_comm_qubits + 1));
            dev->comm_qubit_node_id[i] = dev->num_comm_qubits;
            dev->comm_qubits[dev->num_comm_qubits++] = i;

            core_t comm_qubit_core = dev->phys_to_core[i];
            dev->core_comm_qubits[comm_qubit_core] = realloc(dev->core_comm_qubits[comm_qubit_core], sizeof(pqubit_t) * (dev->core_num_comm_qubits[comm_qubit_core] + 1));
            dev->core_comm_qubits[comm_qubit_core][dev->core_num_comm_qubits[comm_qubit_core]] = i;
            dev->core_num_comm_qubits[comm_qubit_core] += 1;
        }
    }
}


void device_calculate_distance_matrix(device_t *dev) {
    if (dev->distance_matrix != NULL) {
        for (core_t c = 0; c < dev->num_cores; c++) {
            for (pqubit_t i = 0; i < dev->core_capacity; i++) {
                free(dev->distance_matrix[c][i]);
            }
            free(dev->distance_matrix[c]);
        }
        free(dev->distance_matrix);
    }

    dev->distance_matrix = malloc(sizeof(int**) * dev->num_cores);
    int (*edges)[3] = malloc(sizeof(int) * dev->num_edges * 3);
    for (core_t c = 0; c < dev->num_cores; c++) {
        // Build edge list
        int core_edges = 0;
        pqubit_t p_offset = dev->core_qubits[c][0];
        for (int e = 0; e < dev->num_edges; e++) {
            if (dev->phys_to_core[dev->edges[e].p1] == c && dev->phys_to_core[dev->edges[e].p2] == c) {
                edges[core_edges][0] = dev->edges[e].p1 - p_offset;
                edges[core_edges][1] = dev->edges[e].p2 - p_offset;
                edges[core_edges][2] = 1;
                core_edges++;
            }
        }
        // Use Floyd-Warshall algorithm to calculate distances O(num_cores*(num_qubits_in_core^3))
        // In theory you don't need to do it everytime but once for each device
        dev->distance_matrix[c] = floyd_warshall(dev->core_capacity, edges, core_edges);
    }
    free(edges);
}


bool device_has_edge(const device_t* device, pqubit_t p1, pqubit_t p2) {
    if (p1 == p2) return true;  // Self-loop is always present
    if (p1 >= device->num_qubits || p2 >= device->num_qubits) return false;  // Out of bounds

    return device_get_distance(device, p1, p2) == 1; 
}


int device_get_distance(const device_t* device, pqubit_t p1, pqubit_t p2) {
    core_t c1 = device->phys_to_core[p1];
    core_t c2 = device->phys_to_core[p2];

    if (c1 == c2) {
        pqubit_t p_offset = device->core_qubits[c1][0];
        return device->distance_matrix[c1][p1 - p_offset][p2 - p_offset];
    } else {
        return TS_INF;
    }
}


void device_print(const device_t* dev) {
    printf(BHBLU"\nDevice \"%s\":\n"CRESET, dev->name);
    printf(HBLU"  Number of qubits:"CRESET" %d\n", dev->num_qubits);
    printf(HBLU"  Number of cores:"CRESET" %d\n", dev->num_cores);
    printf(HBLU"  Core capacity:"CRESET" %d\n", dev->core_capacity);
    printf(HBLU"  Number of edges:"CRESET" %d\n", dev->num_edges);
    printf(HBLU"  Number of teleport edges:"CRESET" %d\n", dev->num_tp_edges);
    printf(HBLU"  Number of inter-core edges:"CRESET" %d\n", dev->num_intercore_edges);

    // Intercore edges
    printf(HBLU"  Inter-core edges:"CRESET);
    for (int i = 0; i < dev->num_intercore_edges; i++) {
        if (i % 8 == 0) printf("\n    ");
        printf(BLU"("CRESET"%*d"BLU","CRESET"%*d"BLU")  "CRESET, 3, dev->inter_core_edges[i].p1, 3, dev->inter_core_edges[i].p2);
    }
    printf("\n");
}


void device_free(device_t* dev) {
    if (dev->phys_to_core != NULL) free(dev->phys_to_core);
    if (dev->core_qubits != NULL) {
        for (core_t i = 0; i < dev->num_cores; i++) free(dev->core_qubits[i]);
        free(dev->core_qubits);
    }
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    if (dev->edges != NULL) free(dev->edges);
    if (dev->qubit_to_edges != NULL) {
        for (pqubit_t i = 0; i < dev->num_qubits; i++)
            if (dev->qubit_to_edges[i] != NULL) free(dev->qubit_to_edges[i]);
        free(dev->qubit_to_edges);
    }
    if (dev->tp_edges != NULL) free(dev->tp_edges);

    if (dev->comm_qubits != NULL) free(dev->comm_qubits);
    if (dev->core_comm_qubits != NULL) free(dev->core_comm_qubits);
    if (dev->core_num_comm_qubits != NULL) free(dev->core_num_comm_qubits);
    if (dev->qubit_is_comm != NULL) free(dev->qubit_is_comm);

    if (dev->distance_matrix != NULL) {
        for (core_t c = 0; c < dev->num_cores; c++) {
            for (pqubit_t i = 0; i < dev->core_capacity; i++) {
                free(dev->distance_matrix[c][i]);
            }
            free(dev->distance_matrix[c]);
        }
        free(dev->distance_matrix);
    }

    if (dev->json) cJSON_Delete(dev->json);

    free(dev);
}


// Device Instances


device_t* device_a() {
    device_t* dev = device_new_grid(2, 2, 3, 3);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 4;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 5, .p2 = 12};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 16, .p2 = 28};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 7, .p2 = 19};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 23, .p2 = 30};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 3x3Q");
    return dev;
}


device_t* device_b() {
    device_t* dev = device_new_grid(3, 1, 2, 2);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 2;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 3, .p2 = 4};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 7, .p2 = 8};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 3x1Q");
    return dev;
}


device_t* device_c() {
    device_t* dev = device_new_grid(3, 3, 3, 3);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 24;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 2, .p2 = 10};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 8, .p2 = 15};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 11, .p2 = 18};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 17, .p2 = 24};
    dev->inter_core_edges[4] = (device_edge_t){.p1 = 29, .p2 = 36};
    dev->inter_core_edges[5] = (device_edge_t){.p1 = 35, .p2 = 42};
    dev->inter_core_edges[6] = (device_edge_t){.p1 = 38, .p2 = 45};
    dev->inter_core_edges[7] = (device_edge_t){.p1 = 44, .p2 = 51};
    dev->inter_core_edges[8] = (device_edge_t){.p1 = 56, .p2 = 63};
    dev->inter_core_edges[9] = (device_edge_t){.p1 = 62, .p2 = 69};
    dev->inter_core_edges[10] = (device_edge_t){.p1 = 65, .p2 = 72};
    dev->inter_core_edges[11] = (device_edge_t){.p1 = 71, .p2 = 78};
    dev->inter_core_edges[12] = (device_edge_t){.p1 = 6, .p2 = 27};
    dev->inter_core_edges[13] = (device_edge_t){.p1 = 8, .p2 = 29};
    dev->inter_core_edges[14] = (device_edge_t){.p1 = 15, .p2 = 36};
    dev->inter_core_edges[15] = (device_edge_t){.p1 = 17, .p2 = 38};
    dev->inter_core_edges[16] = (device_edge_t){.p1 = 24, .p2 = 45};
    dev->inter_core_edges[17] = (device_edge_t){.p1 = 26, .p2 = 47};
    dev->inter_core_edges[18] = (device_edge_t){.p1 = 33, .p2 = 54};
    dev->inter_core_edges[19] = (device_edge_t){.p1 = 35, .p2 = 56};
    dev->inter_core_edges[20] = (device_edge_t){.p1 = 42, .p2 = 63};
    dev->inter_core_edges[21] = (device_edge_t){.p1 = 44, .p2 = 65};
    dev->inter_core_edges[22] = (device_edge_t){.p1 = 51, .p2 = 72};
    dev->inter_core_edges[23] = (device_edge_t){.p1 = 53, .p2 = 74};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "3x3C 3x3Q");
    return dev;
}

/*
        #   1   2  -  5   6
        #   3   4     7   8
        #   |             |
        #   9  10     13 14
        #   11 12  -  15 16
*/

device_t* device_d() {
    device_t* dev = device_new_grid(2, 2, 2, 2);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 4;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 1, .p2 = 4};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 2, .p2 = 8};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 7, .p2 = 13};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 11, .p2 = 14};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 2x2Q");
    return dev;
}

/*
        #   0  1  2  3    16  17  18  19
        #   4  5  6  7 -  20  21  22  23
        #   8  9 10 11    24  25  26  27
        #  12 13 14 15    28  29  30  31
        #     |                    |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39     52  53  54  55
        #  40 41 42 43  -  56  57  58  59
        #  44 45 46 47     60  61  62  63
*/

device_t* device_e() {
    device_t* dev = device_new_grid(2, 2, 4, 4);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 4;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 13, .p2 = 33};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 7, .p2 = 20};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 30, .p2 = 50};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 43, .p2 = 56};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 4x4Q - E");
    return dev;
}

/*
        #   0  1  2  3  -  16  17  18  19
        #   4  5  6  7     20  21  22  23
        #   8  9 10 11  -  24  25  26  27
        #  12 13 14 15     28  29  30  31
        #  |      |             |       |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39  -  52  53  54  55
        #  40 41 42 43     56  57  58  59
        #  44 45 46 47  -  60  61  62  63

*/

device_t* device_f() {
    device_t* dev = device_new_grid(2, 2, 4, 4);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 8;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 3, .p2 = 16};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 11, .p2 = 24};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 12, .p2 = 32};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 14, .p2 = 34};
    dev->inter_core_edges[4] = (device_edge_t){.p1 = 29, .p2 = 49};
    dev->inter_core_edges[5] = (device_edge_t){.p1 = 31, .p2 = 51};
    dev->inter_core_edges[6] = (device_edge_t){.p1 = 39, .p2 = 52};
    dev->inter_core_edges[7] = (device_edge_t){.p1 = 47, .p2 = 60};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 4x4Q - F");
    return dev;
}

/*
        #   0  1  2  3  -  16  17  18  19
        #   4  5  6  7     20  21  22  23
        #   8  9 10 11     24  25  26  27
        #  12 13 14 15     28  29  30  31
        #  |            X              |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39     52  53  54  55
        #  40 41 42 43     56  57  58  59
        #  44 45 46 47  -  60  61  62  63
*/

device_t* device_g() {
    device_t* dev = device_new_grid(2, 2, 4, 4);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 6;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 3, .p2 = 16};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 12, .p2 = 32};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 31, .p2 = 51};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 47, .p2 = 60};
    dev->inter_core_edges[4] = (device_edge_t){.p1 = 15, .p2 = 48};
    dev->inter_core_edges[5] = (device_edge_t){.p1 = 28, .p2 = 35};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "2x2C 4x4Q - G");
    return dev;
}


device_t* device_h() {
    /*
         0  1  2  3     16  17  18  19     32  33  34  35
         4  5  6  7  -  20  21  22  23  -  36  37  38  39
         8  9 10 11     24  25  26  27     40  41  42  43
        12 13 14 15     28  29  30  31     44  45  46  47
            |                 /                    |
        48 49 50 51     64  65  66  67     80  81  82  83
        52 53 54 55     68  69  70  71     84  85  86  87
        56 57 58 59  -  72  73  74  75  -  88  89  90  91
        60 61 62 63     76  77  78  79     92  93  94  95
    */
    device_t* dev = device_new_grid(3, 2, 4, 4);
    if (dev->inter_core_edges != NULL) free(dev->inter_core_edges);
    dev->num_intercore_edges = 7;
    dev->inter_core_edges = malloc(sizeof(device_edge_t) * dev->num_intercore_edges);

    dev->inter_core_edges[0] = (device_edge_t){.p1 = 13, .p2 = 49};
    dev->inter_core_edges[1] = (device_edge_t){.p1 = 7, .p2 = 20};
    dev->inter_core_edges[2] = (device_edge_t){.p1 = 23, .p2 = 36};
    dev->inter_core_edges[3] = (device_edge_t){.p1 = 59, .p2 = 72};
    dev->inter_core_edges[4] = (device_edge_t){.p1 = 30, .p2 = 65};
    dev->inter_core_edges[5] = (device_edge_t){.p1 = 75, .p2 = 88};
    dev->inter_core_edges[6] = (device_edge_t){.p1 = 46, .p2 = 82};

    device_update_qubit_to_edges(dev);
    device_build_teleport_edges(dev);
    strcpy(dev->name, "3x2C 4x4Q - H");
    return dev;
}