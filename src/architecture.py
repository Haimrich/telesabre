import networkx as nx

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
        self.name = "arch"
        
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
    
    def get_core_distance_matrix(self):
        core_graph = nx.Graph()
        core_graph.add_edges_from([(self.get_qubit_core(edge.p1), self.get_qubit_core(edge.p2)) for edge in self.inter_core_edges])
        return nx.floyd_warshall_numpy(core_graph, nodelist=range(self.num_cores), weight=None)
    
            
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
        
        arch.name = "2x2C 3x3Q"
        return arch
    
    @staticmethod
    def B():
        arch = Architecture(2,2,3,1)
        
        arch.inter_core_edges = [
            Edge(3,4),
            Edge(7,8)
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
        
        arch.name = "2x2C 3x1Q"
        return arch
    
    @staticmethod
    def C():
        arch = Architecture(3,3,3,3)
        
        # 1  2  3  - 10 11 12 - 19 20 21
        # 4  5  6    13 14 15   22 23 24
        # 7  8  9  - 16 17 18 - 25 26 27
        # |     |    |     |    |     |
        # 28 29 30 - 37 38 39 - 46 47 48
        # 31 32 33   40 41 42   49 50 51
        # 34 35 36 - 43 44 45 - 52 53 54
        # |     |    |     |    |     |
        # 55 56 57 - 64 65 66 - 73 74 75
        # 58 59 60   67 68 69   76 77 78
        # 61 62 63 - 70 71 72 - 79 80 81
        
        arch.inter_core_edges = [
            Edge(2,10),
            Edge(8,15),
            Edge(11,18),
            Edge(17,24),
            Edge(29,36),
            Edge(35,42),
            Edge(38,45),
            Edge(44,51),
            Edge(56,63),
            Edge(62,69),
            Edge(65,72),
            Edge(71,78),
            Edge(6,27),
            Edge(8,29),
            Edge(15,36),
            Edge(17,38),
            Edge(24,45),
            Edge(26,47),
            Edge(33,54),
            Edge(35,56),
            Edge(42,63),
            Edge(44,65),
            Edge(51,72),
            Edge(53,74),
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
        
        arch.name = "3x3C 3x3Q"
        return arch
    
    
    @staticmethod
    def D():
        arch = Architecture(2,2,2,2)
        
        #   1   2  -  5   6
        #   3   4     7   8
        #   |             |
        #   9  10     13 14
        #   11 12  -  15 16
        
        
        arch.inter_core_edges = [
            Edge(1,4),
            Edge(2,8),
            Edge(7,13),
            Edge(11,14)
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
        
        arch.name = "2x2C 2x2Q"
        return arch
    
    
    def E():
        arch = Architecture(4,4,2,2)
        
        #   0  1  2  3    16  17  18  19
        #   4  5  6  7 -  20  21  22  23
        #   8  9 10 11    24  25  26  27
        #  12 13 14 15    28  29  30  31
        #     |                    |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39     52  53  54  55
        #  40 41 42 43  -  56  57  58  59
        #  44 45 46 47     60  61  62  63
        
        
        arch.inter_core_edges = [
            Edge(13,33),
            Edge(7,20),
            Edge(30,50),
            Edge(43,56)
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
        
        arch.name = "2x2C 4x4Q - E"
        return arch
        
    

    def F():
        arch = Architecture(4,4,2,2)
        
        #   0  1  2  3  -  16  17  18  19
        #   4  5  6  7     20  21  22  23
        #   8  9 10 11  -  24  25  26  27
        #  12 13 14 15     28  29  30  31
        #  |      |             |       |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39  -  52  53  54  55
        #  40 41 42 43     56  57  58  59
        #  44 45 46 47  -  60  61  62  63
        
        
        arch.inter_core_edges = [
            Edge(3,16),
            Edge(11,24),
            Edge(12,32),
            Edge(14,34),
            Edge(29,49),
            Edge(31,51),
            Edge(39,52),
            Edge(47,60)
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
        
        arch.name = "2x2C 4x4Q - F"
        return arch
    
    
    
    def G():
        arch = Architecture(4,4,2,2)
        
        #   0  1  2  3  -  16  17  18  19
        #   4  5  6  7     20  21  22  23
        #   8  9 10 11     24  25  26  27
        #  12 13 14 15     28  29  30  31
        #  |            X              |
        #  32 33 34 35     48  49  50  51
        #  36 37 38 39     52  53  54  55
        #  40 41 42 43     56  57  58  59
        #  44 45 46 47  -  60  61  62  63
        
        
        arch.inter_core_edges = [
            Edge(3,16),
            Edge(12,32),
            Edge(31,51),
            Edge(47,60),
            Edge(15,48),
            Edge(28,35)
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
        
        arch.name = "2x2C 4x4Q - G"
        return arch
    
    def H():
        arch = Architecture(4,4,3,2)
        
        #   0  1  2  3     16  17  18  19     32  33  34  35
        #   4  5  6  7  -  20  21  22  23  -  36  37  38  39
        #   8  9 10 11     24  25  26  27     40  41  42  43
        #  12 13 14 15     28  29  30  31     44  45  46  47
        #      |                 /                    |
        #  48 49 50 51     64  65  66  67     80  81  82  83  
        #  52 53 54 55     68  69  70  71     84  85  86  87
        #  56 57 58 59  -  72  73  74  75  -  88  89  90  91
        #  60 61 62 63     76  77  78  79     92  93  94  95
        
        
        arch.inter_core_edges = [
            Edge(13,49),
            Edge(7,20),
            Edge(23,36),
            Edge(59,72),
            Edge(30,65),
            Edge(75,88),
            Edge(46,82)
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
        
        arch.name = "3x2C 4x4Q - H"
        return arch
        