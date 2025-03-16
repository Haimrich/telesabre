import random
from itertools import permutations

import networkx as nx

class Gate:
    def __init__(self, target_qubits, gate_type):
        self.target_qubits = target_qubits
        self.type = gate_type

    def shares_target_qubits(self, other):
        return any(q in other.target_qubits for q in self.target_qubits)
    
    def is_two_qubit(self):
        return len(self.target_qubits) == 2


class Circuit:
    def __init__(self, gates=None, dependencies=None, num_qubits=0, num_gates=0, single_qubit_gate_prob=0.0, seed=0, qasm=None, name="random"):
        self.name = name
        self.num_qubits = num_qubits
        self.gates = gates if gates else []
        self.dependencies = dependencies if dependencies else []
        self.dag = nx.DiGraph()
        
        if qasm is not None:
            self.gates, self.num_qubits = Circuit.parse_qasm(qasm)
            print(f"Parsed QASM circuit with {len(self.gates)} gates.")
            self.generate_dependencies()
        elif gates:
            if num_qubits is None:
                self.compute_num_qubits()
            if dependencies is None:
                self.generate_dependencies()
        elif num_gates > 0:
            self.generate_gates(num_gates, single_qubit_gate_prob, seed)
            self.generate_dependencies()
        
        self.dag.add_nodes_from(range(len(self.gates)))
        self.dag.add_edges_from(self.dependencies)
        for layer, nodes in enumerate(nx.topological_generations(self.dag)):
            for node in nodes:
                self.dag.nodes[node]["layer"] = layer
                
        self.num_gates = len(self.gates)
                

    def compute_num_qubits(self):
        max_qubit = 0
        for gate in self.gates:
            for q in gate.target_qubits:
                max_qubit = max(max_qubit, q)
        self.num_qubits = max_qubit + 1


    def generate_dependencies(self):
        self.dependencies.clear()
        last_gate_per_qubit = [-1] * self.num_qubits

        for g, gate in enumerate(self.gates):
            for q in gate.target_qubits:
                if last_gate_per_qubit[q] != -1:
                    self.dependencies.append((last_gate_per_qubit[q], g))
                last_gate_per_qubit[q] = g
                

    def generate_gates(self, num_gates, single_qubit_gate_prob=0.0, seed=0):
        self.gates.clear()
        rng = random.Random(seed)
        possible_cx_gates = list(permutations(range(self.num_qubits), 2))

        for _ in range(num_gates):
            if rng.random() < single_qubit_gate_prob:
                gate = Gate([rng.randint(0, self.num_qubits - 1)], "h")
            else:
                gate = Gate(rng.choice(possible_cx_gates), "cx")
            self.gates.append(gate)
    
    
    @staticmethod
    def from_qiskit(circuit):
        gates = []
        for instruction in circuit.data:
            qubits = [circuit.find_bit(qubit).index for qubit in instruction.qubits]
            if len(qubits) > 2:
                raise ValueError("Only one and two-qubit gates are supported.")
            if instruction.name == "measure":
                continue
            gate = Gate(qubits, instruction.operation.name)
            gates.append(gate)
        return Circuit(gates=gates, num_qubits=circuit.num_qubits)