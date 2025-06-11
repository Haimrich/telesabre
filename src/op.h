#pragma once

#include "device.h"

typedef enum op_type {
    OP_SWAP,
    OP_TELEPORT,
    OP_TELEGATE,
    OP_NONE
} op_type_t;

typedef enum op_target {
    OP_SOURCE      = 0,
    OP_TARGET_A    = 0,
    OP_MEDIATOR    = 1,
    OP_MEDIATOR_A  = 1,
    OP_MEDIATOR_B  = 2,
    OP_TARGET      = 3,
    OP_TARGET_B    = 3,
} op_target_t;

typedef struct op {
    op_type_t type;
    pqubit_t qubits[4];
    int front_gate_idx;
    unsigned char reasons;
} op_t;


static int op_get_num_qubits(const op_t* op) {
    switch (op->type) {
        case OP_TELEPORT:
            return 3;
        case OP_TELEGATE:
            return 4;
        case OP_SWAP:
            return 2;
        default:
            return 0;
    }
}

static const char* op_get_type_str(const op_t* op) {
    switch (op->type) {
        case OP_TELEPORT:
            return "TELEPORT";
        case OP_TELEGATE:
            return "TELEGATE";
        case OP_SWAP:
            return "SWAP";
        default:
            return "UNKNOWN";
    }
}
