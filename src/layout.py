import numpy as np

class Layout:
    def __init__(self, mapping, phys_to_core=None, num_virtual=None):
        self.phys_to_virt = np.array(mapping, dtype=int)
        self.virt_to_phys = np.argsort(self.phys_to_virt)  # Compute inverse
        self.phys_to_core = np.array(phys_to_core, dtype=int) if phys_to_core is not None else np.zeros_like(mapping)
        self.num_virtual = num_virtual if num_virtual is not None else len(mapping)

    def swap(self, phys1, phys2):
        """Swap two physical positions and update the inverse mapping."""
        virt1, virt2 = self.phys_to_virt[phys1], self.phys_to_virt[phys2]
        self.phys_to_virt[phys1], self.phys_to_virt[phys2] = virt2, virt1
        self.virt_to_phys[virt1], self.virt_to_phys[virt2] = phys2, phys1
        
    def teleport(self, phys_source, phys_mediator, phys_target):
        """Teleport a qubit from source to target"""
        assert (not self.is_phys_free(phys_source)) and self.is_phys_free(phys_mediator) and self.is_phys_free(phys_target)
        self.swap(phys_source, phys_target)

    def can_execute_gate(self, gate, graph):
        """Check if the physical positions of virt1 and virt2 are adjacent in a given graph."""
        if not gate.is_two_qubit():
            return True
        virt1, virt2 = gate.target_qubits
        phys1, phys2 = self.virt_to_phys[virt1], self.virt_to_phys[virt2]
        # Check adjacency in the graph
        return graph.has_edge(phys1, phys2) or graph.has_edge(phys2, phys1)
    
    def get_phys(self, virt):
        """Return the physical position of a virtual qubit."""
        return self.virt_to_phys[virt]

    def get_virt(self, phys):
        """Return the virtual qubit mapped to a physical position."""
        return self.phys_to_virt[phys]
    
    def is_phys_free(self, phys):
        """Check if a physical position is free."""
        return self.phys_to_virt[phys] >= self.num_virtual
    
    def get_free_qubits(self):
        """Return a list of free physical qubits."""
        return np.where(self.phys_to_virt >= self.num_virtual)[0]
    
    def get_virt_core(self, virt):
        """Return the core of a virtual qubit."""
        return self.phys_to_core[self.virt_to_phys[virt]]
    
    def get_core_capacity(self, core):
        """Return the current number of free physical positions in a core."""
        return np.sum((self.phys_to_core == core) & (self.phys_to_virt >= self.num_virtual))
    
    def __repr__(self):
        return f"Phys to Virt: {self.phys_to_virt}\nVirt to Phys: {self.virt_to_phys}"

