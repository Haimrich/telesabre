


class Edge:
    # Physical qubits, undirected edge
    def __init__(self, p1, p2):
        self.p1 = p1
        self.p2 = p2


class TeleportEdge:
    # Physical qubits, directed triadic hyperedge
    def __init__(self, p_source, p_mediator, p_target):
        self.p_source = p_source
        self.p_mediator = p_mediator
        self.p_target = p_target


class Architecture:
    def __init__(self, grid_x=None, grid_y=None, core_x=None, core_y=None, double_tp=False):
        self.num_qubits = 0
        self.edges = []
        self.qubit_to_edges = []
        self.inter_core_edges = []
        self.teleport_edges = []
        self.communication_qubits = []
        self.qubit_to_teleport_edges_as_source = []
        self.qubit_to_teleport_edges_as_mediator = []
        self.qubit_to_teleport_edges_as_target = []
        self.qubit_to_teleport_edges = []
        
        self.swap_duration = 3
                 
        # Old TP Modeling
        #self.tp_epr_duration = 1
        #self.tp_preprocess_duration = 2
        #self.tp_measure_duration = 1
        #self.tp_phone_call_duration = 0
        #self.tp_postprocess_duration = 1
        #self.teleport_duration = (self.tp_epr_duration + self.tp_preprocess_duration + 
        #                          self.tp_measure_duration + self.tp_phone_call_duration + self.tp_postprocess_duration)
        
        # New TP Modeling
        self.tp_source_busy_offset = 1
        self.tp_source_busy_duration = 3
        
        self.tp_mediator_busy_offset = 0
        self.tp_mediator_busy_duration = 3
        
        self.tp_target_busy_offset = 0
        self.tp_target_busy_duration = 5
        
        self.teleport_duration = max(self.tp_source_busy_offset + self.tp_source_busy_duration, 
                                     self.tp_mediator_busy_offset + self.tp_mediator_busy_duration, 
                                     self.tp_target_busy_offset + self.tp_target_busy_duration)
        
        if grid_x is not None and grid_y is not None and core_x is not None and core_y is not None:
            self._init_with_cores(grid_x, grid_y, core_x, core_y, double_tp)
        elif grid_x is not None and grid_y is not None:
            self._init_grid(grid_x, grid_y)
            
        if core_x is not None and core_y is not None:
            self.num_cores = core_x * core_y
        else:
            self.num_cores = 1
            
        self.num_edges = len(self.edges)
        self.num_tp_edges = len(self.teleport_edges)
        
        self.communication_qubits = list(set(self.communication_qubits))
        
        self.core_comm_qubits = [[] for _ in range(self.num_cores)]
        for p in self.communication_qubits:
            self.core_comm_qubits[self.qubit_to_core[p]].append(p)
            
        self.core_qubits = [[] for _ in range(self.num_cores)]
        for p in range(self.num_qubits):
            self.core_qubits[self.qubit_to_core[p]].append(p)
        
            

    def _init_grid(self, grid_x, grid_y):
        self.qubit_to_core = [0] * (grid_x * grid_y)
        for y in range(grid_y):
            for x in range(grid_x):
                node_index = y * grid_x + x

                if x < grid_x - 1:
                    self.edges.append(Edge(node_index, node_index + 1))

                if y < grid_y - 1:
                    self.edges.append(Edge(node_index, node_index + grid_x))

        self.num_qubits = grid_x * grid_y
        self._update_qubit_to_edges()

    def _init_with_cores(self, grid_x, grid_y, core_x, core_y, double_tp=False):
        self.qubit_to_core = []
        self.core_qubits = []
        for cy in range(core_y):
            for cx in range(core_x):
                core_start = (cy * core_x + cx) * grid_x * grid_y  # First qubit in the core
                self.core_qubits.append([])
                
                # Teleport edges (connecting core boundaries)
                if cx < core_x - 1:  # Horizontal teleport connection
                    right_core_start = (cy * core_x + (cx + 1)) * grid_x * grid_y
                    self.inter_core_edges.append(Edge(core_start, right_core_start))
                
                if cy < core_y - 1:  # Vertical teleport connection
                    bottom_core_start = ((cy + 1) * core_x + cx) * grid_x * grid_y
                    self.inter_core_edges.append(Edge(core_start, bottom_core_start))
                    
                if double_tp:
                    if cx < core_x - 1:
                        right_core_start = (cy * core_x + (cx + 1)) * grid_x * grid_y
                        self.inter_core_edges.append(Edge(core_start + 1, right_core_start + 1))
                    if cy < core_y - 1:
                        bottom_core_start = ((cy + 1) * core_x + cx) * grid_x * grid_y
                        self.inter_core_edges.append(Edge(core_start + 1, bottom_core_start + 1))
                
                # Intra-core grid edges
                for y in range(grid_y):
                    for x in range(grid_x):
                        node_index = core_start + y * grid_x + x
                        
                        self.qubit_to_core.append(cy * core_x + cx)
                        self.core_qubits[-1].append(node_index)

                        if x < grid_x - 1:  # Connect to right neighbor within the core
                            self.edges.append(Edge(node_index, node_index + 1))
                        
                        if y < grid_y - 1:  # Connect to bottom neighbor within the core
                            self.edges.append(Edge(node_index, node_index + grid_x))

        self.num_qubits = grid_x * grid_y * core_x * core_y
        self._update_qubit_to_edges()
        self._build_teleport_edges()

    def _update_qubit_to_edges(self):
        self.qubit_to_edges.clear()
        self.qubit_to_edges = [[] for _ in range(self.num_qubits)]

        for i, edge in enumerate(self.edges):
            self.qubit_to_edges[edge.p1].append(i)
            self.qubit_to_edges[edge.p2].append(i)

    def _build_teleport_edges(self):
        self.teleport_edges.clear()
        self.communication_qubits.clear()

        for edge in self.inter_core_edges:
            p1, p2 = edge.p1, edge.p2
            
            # Comm. Qubits
            self.communication_qubits.append(p1)
            self.communication_qubits.append(p2)
            
            # Forward direction
            for e in self.qubit_to_edges[p1]:
                p1_neighbor = self.edges[e].p1 if self.edges[e].p1 != p1 else self.edges[e].p2
                self.teleport_edges.append(TeleportEdge(p_source=p1_neighbor, p_mediator=p1, p_target=p2))

            # Reverse direction
            for e in self.qubit_to_edges[p2]:
                p2_neighbor = self.edges[e].p1 if self.edges[e].p1 != p2 else self.edges[e].p2
                self.teleport_edges.append(TeleportEdge(p_source=p2_neighbor, p_mediator=p2, p_target=p1))

        # Initialize teleport edge mappings
        self.qubit_to_teleport_edges_as_source = [[] for _ in range(self.num_qubits)]
        self.qubit_to_teleport_edges_as_mediator = [[] for _ in range(self.num_qubits)]
        self.qubit_to_teleport_edges_as_target = [[] for _ in range(self.num_qubits)]
        self.qubit_to_teleport_edges = [[] for _ in range(self.num_qubits)]

        # Populate teleport edge mappings
        for e, teleport_edge in enumerate(self.teleport_edges):
            self.qubit_to_teleport_edges_as_source[teleport_edge.p_source].append(e)
            self.qubit_to_teleport_edges_as_mediator[teleport_edge.p_mediator].append(e)
            self.qubit_to_teleport_edges_as_target[teleport_edge.p_target].append(e)
            self.qubit_to_teleport_edges[teleport_edge.p_source].append(e)
            self.qubit_to_teleport_edges[teleport_edge.p_mediator].append(e)
            self.qubit_to_teleport_edges[teleport_edge.p_target].append(e)
    
    def is_comm_qubit(self, qubit):
        return qubit in self.communication_qubits
    
    def get_qubit_core(self, qubit):
        return self.qubit_to_core[qubit]
    
            
    @staticmethod
    def A():
        arch = Architecture(3,3,2,2)
        
        arch.inter_core_edges = [
            Edge(5,12),
            Edge(16,28),
            Edge(7,19),
            Edge(23,30)
        ]
        
        arch._update_qubit_to_edges()
        arch._build_teleport_edges()
        
        arch.communication_qubits = list(set(arch.communication_qubits))
        
        arch.core_comm_qubits = [[] for _ in range(arch.num_cores)]
        for p in arch.communication_qubits:
            arch.core_comm_qubits[arch.qubit_to_core[p]].append(p)
            
        arch.core_qubits = [[] for _ in range(arch.num_cores)]
        for p in range(arch.num_qubits):
            arch.core_qubits[arch.qubit_to_core[p]].append(p)
        
        return arch
    
    @staticmethod
    def B():
        arch = Architecture(2,2,3,1)
        
        arch.inter_core_edges = [
            Edge(3,4),
            Edge(7,8)
        ]
        
        arch._update_qubit_to_edges
        arch._build_teleport_edges()
        
        arch.communication_qubits = list(set(arch.communication_qubits))
        
        arch.core_comm_qubits = [[] for _ in range(arch.num_cores)]
        for p in arch.communication_qubits:
            arch.core_comm_qubits[arch.qubit_to_core[p]].append(p)
            
        arch.core_qubits = [[] for _ in range(arch.num_cores)]
        for p in range(arch.num_qubits):
            arch.core_qubits[arch.qubit_to_core[p]].append(p)
        
        return arch
    
        
    
