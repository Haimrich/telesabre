#!/usr/bin/env python3

import argparse
import os

from circuit import Circuit
from architecture import Architecture

from qlimt import sabre_mapping
from sabre import run_sabre


def main():
    args = parse_args()
    
    qasm_filename = args.qasm
    if qasm_filename is not None:
        from qiskit import qasm2
        qiskit_circuit = qasm2.load(qasm_filename,  custom_instructions=qasm2.LEGACY_CUSTOM_INSTRUCTIONS)
        circuit = Circuit.from_qiskit(qiskit_circuit)
        circuit.name = os.path.basename(qasm_filename).split(".")[0]
    else:
        circuit = Circuit(num_qubits=args.num_qubits, num_gates=args.num_gates, single_qubit_gate_prob=args.one_qubit_gate_prob, seed=args.seed)
    
    print("=== Circuit ===")
    print(" Number of gates is", circuit.num_gates)
    print(" Number of qubits is", circuit.num_qubits)
    
    if args.architecture is not None:
        if args.architecture == "a":
            architecture = Architecture.A()
        elif args.architecture == "b":
            architecture = Architecture.B()
        else:
            raise ValueError("Invalid architecture")
    else:
        architecture = Architecture(grid_x=args.qubit_x, grid_y=args.qubit_y, core_x=args.core_x, core_y=args.core_y, double_tp=args.double_tp)
    
    print("=== Architecture ===")
    print(" Num. Qubits:", architecture.num_qubits)
    print(" Num. Edges:", architecture.num_edges)
    print(" Num. TP Edges:", architecture.num_tp_edges)
    print(" Num. Cores:", architecture.num_cores)
    
    print("=== QLIMT ===")
    
    swaps, tps, telegate, depth = sabre_mapping(circuit, architecture)
    
    print("Swaps:", swaps)
    print("Teleports:", tps)
    print("Teleport Gates:", telegate)
    print("Depth:", depth)
    
    if architecture.num_cores == 1:
        print("=== SABRE ===")
        swaps, depth = run_sabre(circuit, architecture)
        print("Swaps:", swaps)
        print("Depth:", depth)
    
    
    


def parse_args():
    args = argparse.ArgumentParser()
    args.add_argument("--qasm", type=str, default=None)
    args.add_argument("--num_qubits", type=int, default=8)
    args.add_argument("--num_gates", type=int, default=16)
    args.add_argument("--seed", type=int, default=42)
    args.add_argument("--one_qubit_gate_prob", type=float, default=0.1)
    args.add_argument("--architecture", type=str, default=None)
    args.add_argument("--core_x", type=int, default=2)
    args.add_argument("--core_y", type=int, default=2)
    args.add_argument("--qubit_x", type=int, default=2)
    args.add_argument("--qubit_y", type=int, default=2)
    args.add_argument("--double_tp", action='store_true')
    return args.parse_args()

if __name__ == "__main__":
    main()