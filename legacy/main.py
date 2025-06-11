#!/usr/bin/env python3

import argparse
import os

import numpy as np

from circuit import Circuit
from architecture import Architecture
from config import Config

from sota import hungarian_assignement, count_non_valid_assignments, count_intercore_comms, fgp_oee_assignment
from qlimt import run_telesabre
from sabre import run_sabre



def main():
    args = parse_args()
    
    print("=== Configuration ===")
    config = Config()
    print(config)
    
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
        elif args.architecture == "c":
            architecture = Architecture.C()
        elif args.architecture == "d":
            architecture = Architecture.D()
        elif args.architecture == "e":
            architecture = Architecture.E()
        elif args.architecture == "f":
            architecture = Architecture.F()
        elif args.architecture == "g":
            architecture = Architecture.G()
        elif args.architecture == "h":
            architecture = Architecture.H()
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
    
    swaps, tps, telegate, depth, tp_depth, deadlocks, initial_layout = run_telesabre(config, circuit, architecture, seed= args.seed)
    
    if architecture.num_cores == 1:
        print("=== SABRE ===")
        swaps, depth = run_sabre(circuit, architecture)
        print("Swaps:", swaps)
        print("Depth:", depth)
    
    
    slices = circuit.get_slices()
    capacity = architecture.num_qubits // architecture.num_cores
    core_distance_matrix = architecture.get_core_distance_matrix()
    
    print("=== Hungarian ===")
    print(core_distance_matrix)
    print(capacity)
    
    
    assignments = hungarian_assignement(slices, architecture.num_qubits, architecture.num_cores, capacity, lookahead=True, distance_matrix=core_distance_matrix)
    non_valid, exceed_cap, sep_friends = count_non_valid_assignments(slices, assignments, capacity=capacity)
    hun_teleports, hun_depth = count_intercore_comms(assignments, core_distance_matrix)
    print("  Teleports:", hun_teleports)
    print("  Depth:", hun_depth)
    print("  Non-valid assignments:", non_valid)
    
    print("=== Hungarian Same Initial Layout ===")
    
    initial_layout = np.array([architecture.get_qubit_core(initial_layout.virt_to_phys[v]) for v in range(architecture.num_qubits)])
    assignments = hungarian_assignement(slices, architecture.num_qubits, architecture.num_cores, capacity, lookahead=True, distance_matrix=core_distance_matrix, initial=initial_layout)
    non_valid, exceed_cap, sep_friends = count_non_valid_assignments(slices, assignments, capacity=capacity)
    hun_teleports, hun_depth = count_intercore_comms(assignments, core_distance_matrix)
    print("  Teleports:", hun_teleports)
    print("  Depth:", hun_depth)
    print("  Non-valid assignments:", non_valid, "Exceed cap:", exceed_cap, "Sep friends:", sep_friends)

    
    print("Slices", slices)
    print("Timesteps:", len(slices))
    


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