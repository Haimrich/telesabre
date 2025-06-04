#include"circuit.h"

#include <stdlib.h>
#include <stdbool.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>


circuit_t* parse_qasm_file(const char* filename) 
{
    regex_t regex;
    char regex_str[] = "([[:alnum:]_]*)(\\([[:alnum:]_\\./-]*\\))* ([[:alnum:]_]*)\\[([0-9]*)\\](,([[:alnum:]_]+)\\[([0-9]*)\\])*;";
    int ret = regcomp(&regex, regex_str, REG_EXTENDED);
    if (ret) {
        char error_buffer[100];
        regerror(ret, &regex, error_buffer, sizeof(error_buffer));
        fprintf(stderr, "Regex compilation failed: %s\n", error_buffer);
        return NULL;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return NULL;
    }

    char line[QASM_MAX_LINE_LENGTH];
    circuit_t *circuit = malloc(sizeof(circuit_t));
    circuit->num_qubits = 0;
    circuit->num_gates = 0;
    circuit->gates = NULL;

    regmatch_t matches[8];
    
    char** qregs = NULL;
    int* qregs_sizes = NULL;
    int num_qregs = 0;


    while(fgets(line, QASM_MAX_LINE_LENGTH, file)) {
        ret = regexec(&regex, line, 8, matches, 0);
        if (!ret) {
            // Instruction type 
            char type[32];
            if (matches[1].rm_so != -1) {
                int length = matches[1].rm_eo - matches[1].rm_so;
                strncpy(type, line + matches[1].rm_so, length);
                type[length] = '\0';
            } else {
                fprintf(stderr, "Error in parsing instruction type in QASM\n");
                exit(1);
            }

            // Target register
            char reg[32];
            if (matches[3].rm_so != -1) {
                int length = matches[3].rm_eo - matches[3].rm_so;
                strncpy(reg, line + matches[3].rm_so, length);
                reg[length] = '\0';
            } else {
                fprintf(stderr, "Error in parsing target register in QASM\n");
                exit(1);
            }

            // Target qubit
            char qubit[32];
            if (matches[4].rm_so != -1) {
                int length = matches[4].rm_eo - matches[4].rm_so;
                strncpy(qubit, line + matches[4].rm_so, length);
                qubit[length] = '\0';
            } else {
                fprintf(stderr, "Error in parsing target qubit in QASM\n");
                exit(1);
            }
            size_t qubit_num = atoi(qubit);

            // Optional second register
            char other_reg[32];
            if (matches[6].rm_so != -1) {
                int length = matches[6].rm_eo - matches[6].rm_so;
                strncpy(other_reg, line + matches[6].rm_so, length);
                other_reg[length] = '\0';
            } else {
                other_reg[0] = '\0';  // No second register
            }

            // Optional second qubit
            char other_qubit[32];
            if (matches[7].rm_so != -1) {
                int length = matches[7].rm_eo - matches[7].rm_so;
                strncpy(other_qubit, line + matches[7].rm_so, length);
                other_qubit[length] = '\0';
            } else {
                other_qubit[0] = '\0';  // No second qubit
            }
            size_t other_qubit_num = atoi(other_qubit);

            // Parse the instruction
            if (strcmp(type, "qreg") == 0) {
                qregs = realloc(qregs, sizeof(char*) * (num_qregs + 1));
                qregs[num_qregs] = malloc(strlen(reg) + 1);
                strcpy(qregs[num_qregs], reg);
                qregs_sizes = realloc(qregs_sizes, sizeof(int) * (num_qregs + 1));
                qregs_sizes[num_qregs] = qubit_num;
                num_qregs++;
                circuit->num_qubits += qubit_num;
            } else if (strcmp(type, "creg") && strcmp(type, "barrier") && strcmp(type, "measure")) {
                // Find register
                int qubit_offset = 0;
                for (int i = 0; i < num_qregs; i++) {
                    if (strcmp(qregs[i], reg) == 0) break;
                    qubit_offset += qregs_sizes[i];
                }

                gate_t gate = {0};
                strcpy(gate.type, type);
                gate.id = circuit->num_gates;
                gate.target_qubits[0] = qubit_offset + qubit_num;
                gate.num_target_qubits = 1;

                if (other_reg[0] != '\0') {
                    qubit_offset = 0;
                    for (int i = 0; i < num_qregs; i++) {
                        if (strcmp(qregs[i], other_reg) == 0) break;
                        qubit_offset += qregs_sizes[i];
                    }
                    gate.target_qubits[1] = qubit_offset + other_qubit_num;
                    gate.num_target_qubits++;
                }

                circuit->gates = realloc(circuit->gates, sizeof(gate_t) * (circuit->num_gates + 1));
                circuit->gates[circuit->num_gates++] = gate;
            }

        } else if (ret == REG_NOMATCH) {
            ;
        } else {
            char error_buffer[100];
            regerror(ret, &regex, error_buffer, sizeof(error_buffer));
            fprintf(stderr, "Regex match failed: %s\n", error_buffer);
        }
    }
    fclose(file);

    circuit_build_dependencies(circuit);
    return circuit;
}


void circuit_build_dependencies(circuit_t* circuit) 
{
    size_t *last_gate_per_qubit = malloc(sizeof(size_t) * circuit->num_qubits);
    for (size_t i = 0; i < circuit->num_qubits; i++)
        last_gate_per_qubit[i] = -1;
    
    for (size_t g = 0; g < circuit->num_gates; g++)
    {
        gate_t *gate = &circuit->gates[g];
        for (size_t j = 0; j < gate->num_target_qubits; j++)
        {
            size_t qubit = gate->target_qubits[j];
            size_t last_gate_on_qubit = last_gate_per_qubit[qubit];
            if (last_gate_on_qubit != -1)
            {
                circuit->gates[last_gate_on_qubit].children_id[circuit->gates[last_gate_on_qubit].num_children++] = g;
                gate->num_parents++;
            }
            last_gate_per_qubit[qubit] = g;
        }
    }
    
}


bool gate_is_two_qubit(gate_t *gate)
{
    return gate->num_target_qubits == 2;
}


bool gates_share_qubits(gate_t *gate1, gate_t *gate2)
{
    for (size_t i = 0; i < gate1->num_target_qubits; i++)
        for (size_t j = 0; j < gate2->num_target_qubits; j++)
            if (gate1->target_qubits[i] == gate2->target_qubits[j])
                return true;
    return false;
}


void circuit_print(circuit_t* circuit)
{
    for (size_t i = 0; i < circuit->num_gates; i++)
    {
        gate_t *gate = &circuit->gates[i];
        printf("Gate %zu: %s ", i, gate->type);
        for (size_t j = 0; j < gate->num_target_qubits; j++)
        {
            printf("%d ", gate->target_qubits[j]);
        }
        printf("\n");
    }
}


/*
def slice_circuit_(self) -> tuple:
    self.gates_slice = [None] * self.num_gates
    slices = defaultdict(list)
    
    def add_gate(gate_id, t):
        if gate_id in slices[t]:
            return

        used_qubits_in_slice = set().union(*[self.gates[gid] for gid in slices[t]])
        if any(q in used_qubits_in_slice for q in self.gates[gate_id]):
            t += 1
            slices[t].append(gate_id)
            self.gates_slice[gate_id] = t
            return
        
        if t == 0:
            slices[t].append(gate_id)
            self.gates_slice[gate_id] = t
            return
        
        add_gate(gate_id, t-1)
        
    for g, gate in enumerate(self.gates):
        t = len(list(s for s in slices.values() if s))
        add_gate(g, t)
    
    self.slices = tuple(tuple(slices[t]) for t in range(len(slices)) if slices[t])
    self.num_slices = len(self.slices)
    
    assert None not in self.gates_slice

*/


sliced_circuit_view_t* circuit_get_sliced_view(circuit_t* circuit, bool two_qubit_only) {
    if (!circuit) return NULL;

    sliced_circuit_view_t* view = malloc(sizeof(sliced_circuit_view_t));
    if (!view) return NULL;

    view->circuit = circuit;
    view->num_slices = 1;
    view->slice_sizes = calloc(1, sizeof(size_t)); // [1] slice of size 0
    view->slices = calloc(1, sizeof(size_t*));     // [1] NULL pointer for slice
    view->gate_slices = malloc(sizeof(size_t) * circuit->num_gates);

    // Mark all gates as unassigned
    for (size_t i = 0; i < circuit->num_gates; i++)
        view->gate_slices[i] = (size_t)-1;

    bool* qubit_used_in_slice = calloc(circuit->num_qubits, sizeof(bool));

    for (size_t g = 0; g < circuit->num_gates; g++) {
        if (two_qubit_only && !gate_is_two_qubit(&circuit->gates[g]))
            continue;

        bool allocated = false;
        for (int t = (int)view->num_slices - 1; t >= 0 && !allocated; t--) {
            memset(qubit_used_in_slice, 0, sizeof(bool) * circuit->num_qubits);

            for (size_t gg = 0; gg < view->slice_sizes[t]; gg++) {
                gate_t* ggate = &circuit->gates[view->slices[t][gg]];
                for (size_t j = 0; j < ggate->num_target_qubits; j++)
                    qubit_used_in_slice[ggate->target_qubits[j]] = true;
            }

            size_t tt = t;
            gate_t* gate = &circuit->gates[g];
            for (size_t j = 0; j < gate->num_target_qubits && !allocated; j++) {
                if (qubit_used_in_slice[gate->target_qubits[j]]) {
                    tt = t + 1;
                    if (tt >= view->num_slices) {
                        // Grow slices and slice_sizes
                        size_t* new_slice_sizes = realloc(view->slice_sizes, sizeof(size_t) * (tt + 1));
                        size_t** new_slices = realloc(view->slices, sizeof(size_t*) * (tt + 1));
                        view->slice_sizes = new_slice_sizes;
                        view->slices = new_slices;
                        view->slice_sizes[tt] = 0;
                        view->slices[tt] = NULL;
                        view->num_slices = tt + 1;
                    }
                    // Add g to slice tt
                    size_t* new_slice = realloc(view->slices[tt], sizeof(size_t) * (view->slice_sizes[tt] + 1));
                    view->slices[tt] = new_slice;
                    view->slices[tt][view->slice_sizes[tt]] = g;
                    view->slice_sizes[tt]++;
                    view->gate_slices[g] = tt;
                    allocated = true;
                }
            }
            if (!allocated && tt == 0) {
                // Add to slice 0
                size_t* new_slice = realloc(view->slices[tt], sizeof(size_t) * (view->slice_sizes[tt] + 1));
                view->slices[tt] = new_slice;
                view->slices[tt][view->slice_sizes[tt]] = g;
                view->slice_sizes[tt]++;
                view->gate_slices[g] = tt;
                allocated = true;
            }
        }
    }
    free(qubit_used_in_slice);
    return view;
}


void sliced_circuit_view_print(sliced_circuit_view_t* view)
{
    printf("Sliced Circuit View:\n");
    for (size_t i = 0; i < view->num_slices; i++)
    {
        printf("Slice %zu: ", i);
        for (size_t j = 0; j < view->slice_sizes[i]; j++)
        {
            printf("%zu ", view->slices[i][j]);
        }
        printf("\n");
    }
}




void free_circuit(circuit_t* circuit) 
{
    if (circuit->gates != NULL)
        free(circuit->gates);
    
    free(circuit);
}


void free_sliced_circuit_view(sliced_circuit_view_t* view) 
{
    if (view->slice_sizes != NULL)
        free(view->slice_sizes);

    if (view->gate_slices != NULL)
        free(view->gate_slices);

    if (view->slices != NULL)
    {
        for (size_t i = 0; i < view->num_slices; i++) {
            if (view->slices[i] != NULL)
            free(view->slices[i]);
        }
        free(view->slices);
    }
    
    free(view);
}