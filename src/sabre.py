from qiskit import QuantumCircuit
from qiskit.transpiler import CouplingMap, PassManager
from qiskit.transpiler.passes import *

def run_sabre(circuit, architecture, heuristic="lookahead", seed=0):
    # heuristic can be ["basic", "lookahead", "decay"]
    qc = QuantumCircuit(circuit.num_qubits)
    for gate in circuit.gates:
        if len(gate.target_qubits) == 2:
            qc.cx(*gate.target_qubits)
        elif len(gate.target_qubits) == 1:
            qc.h(gate.target_qubits[0])
        else:
            raise TypeError("Currently only support one and two-qubit gate.")
    
    edges = [[edge.p1, edge.p2] for edge in architecture.edges]
    edges += [[edge.p2, edge.p1] for edge in architecture.edges]
    edges += [[edge.p_mediator, edge.p_target] for edge in architecture.teleport_edges]
    device = CouplingMap(couplinglist=edges)
    
    pass_manager = PassManager([#SabreLayout(coupling_map=device, seed=seed),
                                TrivialLayout(coupling_map=device),
                                FullAncillaAllocation(coupling_map=device),
                                ApplyLayout(),
                                ##EnlargeWithAncilla(), 
                                SabreSwap(coupling_map=device, heuristic=heuristic, seed=seed, trials=1)])
    sabre_cir = pass_manager.run(qc)
    num_swaps = sum(1 for gate in sabre_cir.data if gate[0].name == 'swap')
    sabre_cir = sabre_cir.decompose(['swap'])
    depth = sabre_cir.depth()
    
    #sabre_cir.draw(output='mpl', scale=0.5).savefig("sabre_circuit.png")
    return num_swaps, depth
