
#!/usr/bin/env python3

import shutil
import glob
import os

from circuit import Circuit
from architecture import Architecture
from config import Config

from qlimt import run_telesabre
from sabre import run_sabre
from sota import hungarian_assignement, count_non_valid_assignments, count_intercore_comms, fgp_oee_assignment


import numpy as np


def load_circuit(qasm_filename):
    from qiskit import qasm2
    qiskit_circuit = qasm2.load(qasm_filename,  custom_instructions=qasm2.LEGACY_CUSTOM_INSTRUCTIONS)
    circuit = Circuit.from_qiskit(qiskit_circuit)
    circuit.name = os.path.basename(qasm_filename).split(".")[0]
    return circuit


def main():
    run_hungarian = False
    qasm_filenames = glob.glob("qasm_hun/*.qasm")
    outdir = "telegate_results"
    outfile = outdir + "/results.csv"
    os.makedirs(outdir + "/json/", exist_ok=True)
    with open(outfile, "w") as f:
        f.write("experiment,method,architecture,config,circuit,seed,swaps,teledata,telegate,depth,tp_depth,deadlocks,\n")
    
    # list of architectures
    architectures = [
        Architecture.A(),
    ]
    
    # list of configs
    config_no_telegate = Config(name="no_telegate", telegate_bonus=-1000)
    
    configs = [
        Config(),
        config_no_telegate
    ]
    
    # list of circuits
    circuits = [
        load_circuit(qasm_filename) for qasm_filename in qasm_filenames
    ]
    
    seeds = [1,2,3,4,5]
    
    i = 0
    
    for seed in seeds:
        for arch in architectures:
            for config in configs:
                for circuit in circuits:
                    run_experiment(outfile, arch, config, circuit, seed, i, run_hungarian)
                    #shutil.copyfile("viewer/data.json", f"{outdir}/jsons/data_{i}.json")
                    i += 1
           
    
def run_experiment(outfile, arch, config, circuit, seed, i, run_hungarian):
    method = "TeleSABRE"
    print(f"Experiment {i}: Architecture: {arch.name}, Config: {config.name}, Circuit: {circuit.name}")
    
    swaps, teledata, telegate, depth, tp_depth, deadlocks, initial_layout = run_telesabre(config, circuit, arch, seed= seed)
    
    with open(outfile, "a") as f:
        f.write(f"{i},{method},{arch.name},{config.name},{circuit.name},{seed},{swaps},{teledata},{telegate},{depth},{tp_depth},{deadlocks}\n")

    if arch.num_cores == 1:
        method = "SABRE"
        swaps, depth = run_sabre(circuit, arch)
        with open(outfile, "a") as f:
            f.write(f"{i},SABRE,{arch.name},{config.name},{circuit.name},{seed},{swaps},0,0,{depth},0,0\n")
            
    # hungarian teledata only
    if run_hungarian:
        method = "HQA"
        
        slices = circuit.get_slices()
        capacity = arch.num_qubits // arch.num_cores
        core_distance_matrix = arch.get_core_distance_matrix()
        
        assignments = hungarian_assignement(slices, arch.num_qubits, arch.num_cores, capacity, lookahead=True, distance_matrix=core_distance_matrix)
        non_valid, exceed_cap, sep_friends = count_non_valid_assignments(slices, assignments, capacity=capacity)
        teledata, tp_depth = count_intercore_comms(assignments, core_distance_matrix)
        telegate = 0
        depth = 0
        deadlocks = non_valid
        swaps = 0
        
        with open(outfile, "a") as f:
            f.write(f"{i},{method},{arch.name},{config.name},{circuit.name},{seed},{swaps},{teledata},{telegate},{depth},{tp_depth},{deadlocks}\n")

        # hungarian same initial
        method = "HQA Same Initial"
        initial_layout = np.array([arch.get_qubit_core(initial_layout.virt_to_phys[v]) for v in range(arch.num_qubits)])
        assignments = hungarian_assignement(slices, arch.num_qubits, arch.num_cores, capacity, lookahead=True, distance_matrix=core_distance_matrix, initial=initial_layout)
        deadlocks, exceed_cap, sep_friends = count_non_valid_assignments(slices, assignments, capacity=capacity)
        teledata, tp_depth = count_intercore_comms(assignments, core_distance_matrix)

        with open(outfile, "a") as f:
            f.write(f"{i},{method},{arch.name},{config.name},{circuit.name},{seed},{swaps},{teledata},{telegate},{depth},{tp_depth},{deadlocks}\n")

        

if __name__ == "__main__":
    main()