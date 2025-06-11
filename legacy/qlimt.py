import json

import networkx as nx
import numpy as np

from copy import deepcopy

from layout import Layout
from utils import NpEncoder, SparseBucketPriorityQueue
from plotting import plot_iteration



def get_separated_virt_pairs(front, node_gate, layout):
    separated_pairs = []
    separated_nodes = []
    for node in front:
        gate = node_gate[node]
        if gate.is_two_qubit():
            virt1, virt2 = gate.target_qubits
            core1, core2 = layout.get_virt_core(virt1), layout.get_virt_core(virt2)
            if core1 != core2:
                separated_pairs.append((virt1, virt2))
                separated_nodes.append(node)
    return separated_pairs, separated_nodes



def get_nearest_free_qubits(layout, dist_matrix, target_qubits):
    if not target_qubits:
        return [], []
    free_qubits = layout.get_free_qubits()
    targets_to_free_distances = dist_matrix[np.ix_(target_qubits, free_qubits)]
    nearest_free_to_targets = np.argmin(targets_to_free_distances, axis=1)
    nearest_free_to_targets = free_qubits[nearest_free_to_targets]
    nearest_free_to_targets_distances = np.min(targets_to_free_distances, axis=1)
    return nearest_free_to_targets, nearest_free_to_targets_distances    



def calculate_energy_exponential(config, dag, architecture, layout, nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock):
    energy = 0
    future_energy = 0
    front_energy = 0
    
    # Calculate weighted score using exponential decay based on depth
    for depth, layer in enumerate(nx.topological_generations(dag)):
        lookahead_factor = 0.1 ** (depth/5) if depth > 0 else 100
        traffic = {}
        g = 0.0
        for node in layer:
            if node_to_gate[node].is_two_qubit():
                node_energy = 0
                virt1, virt2 = node_to_gate[node].target_qubits
                phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                core1, core2 = architecture.get_qubit_core(phys1), architecture.get_qubit_core(phys2)
                if core1 == core2:
                    distance = local_distance_matrix[phys1][phys2]
                    node_energy = distance * lookahead_factor * 2 # Apply exponential decay with depth
                else:
                    virts = node_to_gate[node].target_qubits
                    contracted_graph_g = build_contracted_graph_for_virt_pair(architecture, layout, nearest_free_to_comms_queues, local_distance_matrix, full_core_penalty, virts, traffic=traffic)
                    shortest_path = nx.shortest_path(contracted_graph_g, source=phys1, target=phys2, weight='weight')
                    for edge in zip(shortest_path[:-1], shortest_path[1:]):
                        if not architecture.is_comm_qubit(edge[0]) or not architecture.is_comm_qubit(edge[1]):
                            continue
                        if edge in traffic:
                            traffic[edge] += 1
                        else:
                            traffic[edge] = 1
                    distance = sum(contracted_graph_g.edges[edge]['weight'] for edge in zip(shortest_path[:-1], shortest_path[1:]))
                    node_energy = distance * lookahead_factor
                                    
                node_energy = (1 + g / 10) * node_energy
                energy += node_energy
                if depth != 0:
                    future_energy += node_energy
                else:
                    front_energy += node_energy
                g += 1.0                      
        
    energy *= decay
                
    return energy, front_energy, future_energy



def calculate_energy_extended_set(config, dag, architecture, layout, nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock):
    energy = 0
    future_energy = 0
    front_energy = 0
    
    # Calculate considering front and extended set
    front_size = 1
    extended_set_size = 0
    for depth, layer in enumerate(nx.topological_generations(dag)):
        traffic = {}
        g = 0.0
        for node in layer:
            if node_to_gate[node].is_two_qubit():
                node_energy = 0
                virt1, virt2 = node_to_gate[node].target_qubits
                phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                core1, core2 = architecture.get_qubit_core(phys1), architecture.get_qubit_core(phys2)
                if core1 == core2:
                    distance = local_distance_matrix[phys1][phys2]
                    #node_energy = distance * lookahead_factor * 2 # Apply exponential decay with depth
                    node_energy = distance
                else:
                    virts = node_to_gate[node].target_qubits
                    contracted_graph_g = build_contracted_graph_for_virt_pair(architecture, layout, nearest_free_to_comms_queues, local_distance_matrix, full_core_penalty, virts, traffic=traffic)
                    shortest_path = nx.shortest_path(contracted_graph_g, source=phys1, target=phys2, weight='weight')
                    for edge in zip(shortest_path[:-1], shortest_path[1:]):
                        if not architecture.is_comm_qubit(edge[0]) or not architecture.is_comm_qubit(edge[1]):
                            continue
                        if edge in traffic:
                            traffic[edge] += 1
                        else:
                            traffic[edge] = 1
                    distance = sum(contracted_graph_g.edges[edge]['weight'] for edge in zip(shortest_path[:-1], shortest_path[1:]))
                    #node_energy = distance * lookahead_factor
                    node_energy = distance
                                    
                #node_energy = (1 + g / 10) * node_energy
                energy += node_energy
                if depth != 0:
                    future_energy += node_energy
                else:
                    front_energy += node_energy
                g += 1.0
                if solving_deadlock:
                    break
        if solving_deadlock and g > 0:
            break
                
        if depth == 0:
            front_size = max(1, sum(node_to_gate[node].is_two_qubit() for node in layer))
        else:
            extended_set_size += sum(node_to_gate[node].is_two_qubit() for node in layer)
        if extended_set_size > config.extended_set_size:
            break
                
    # Apply decay factor to score
    energy = front_energy / front_size
    if extended_set_size > 0:
        energy += 0.05 * future_energy / extended_set_size                
        
    energy *= decay
                
    return energy, front_energy, future_energy



def calculate_energy(config, dag, architecture, layout, nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock):
    if config.energy_type == "extended_set":
        return calculate_energy_extended_set(config, dag, architecture, layout, nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock)
    elif config.energy_type == "exponential":
        return calculate_energy_exponential(config, dag, architecture, layout, nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock)
    else:
        raise ValueError(f"Unknown energy type: {config.energy_type}")



def initial_layout(config, circuit, architecture):
    """Builds naive initial layout that allows teleports (no core with capacity < 2)."""
    
    if config.initial_layout_hun_like:
        
        virt_to_core = np.full(architecture.num_qubits, -1)
        capacities = np.full(architecture.num_cores, architecture.num_qubits // architecture.num_cores)
        for q1, q2 in circuit.get_slices()[0]:
            for c in range(architecture.num_cores):
                if capacities[c] > 4:
                    virt_to_core[q1] = c
                    virt_to_core[q2] = c
                    capacities[c] -= 2
                    break
        for q in range(circuit.num_qubits):
            if virt_to_core[q] == -1:
                for c in range(architecture.num_cores):
                    if capacities[c] > 3:
                        virt_to_core[q] = c
                        capacities[c] -= 1
                        break
                    
        core_to_virt = [[] for c in range(architecture.num_cores)] 
        for c in range(architecture.num_cores):
            for q in range(architecture.num_qubits):
                if virt_to_core[q] == c:
                    core_to_virt[c].append(q)
        
        permutation = np.random.permutation(architecture.num_qubits)
        virt_empty = circuit.num_qubits
        phys_to_virt = [0] * architecture.num_qubits
        for p in permutation:
            core = architecture.get_qubit_core(p)
            if len(core_to_virt[core]) > 0:
                phys_to_virt[p] = core_to_virt[core][-1]
                core_to_virt[core].remove(core_to_virt[core][-1])
            else:
                phys_to_virt[p] = virt_empty
                virt_empty += 1
                
        assert len(np.unique(phys_to_virt)) == architecture.num_qubits
    else:
        core_capacities = [architecture.num_qubits // architecture.num_cores] * architecture.num_cores
        phys_to_virt = [0] * architecture.num_qubits
        virt = 0
        virt_empty = circuit.num_qubits
        
        permutation = np.random.permutation(architecture.num_qubits)
        #permutation = np.arange(architecture.num_qubits)
        
        for p in permutation:
            core = architecture.get_qubit_core(p)
            if core_capacities[core] > 1 and virt < circuit.num_qubits:
                core_capacities[core] -= 1
                phys_to_virt[p] = virt
                virt += 1
            else:
                phys_to_virt[p] = virt_empty
                virt_empty += 1
            
    return Layout(phys_to_virt, architecture.qubit_to_core, circuit.num_qubits)



def calculate_global_distance_matrix(architecture):
    basic_edges = [(e.p1, e.p2) for e in architecture.edges]
    teleport_edges = []
    # connect p1 neighbours to p2 neighbours
    for e in architecture.inter_core_edges:
        for e_p1 in architecture.qubit_to_edges[e.p1]:
            neighbour_p1 = architecture.edges[e_p1].p1 if architecture.edges[e_p1].p1 != e.p1 else architecture.edges[e_p1].p2
            for e_p2 in architecture.qubit_to_edges[e.p2]:
                neighbour_p2 = architecture.edges[e_p2].p1 if architecture.edges[e_p2].p1 != e.p2 else architecture.edges[e_p2].p2
                teleport_edges.append((neighbour_p1, neighbour_p2))       
    
    distance_graph = nx.empty_graph(architecture.num_qubits)
    distance_graph.add_edges_from(basic_edges + teleport_edges)
    
    # Calculate all-pairs shortest path distances
    return nx.floyd_warshall_numpy(distance_graph, nodelist=range(architecture.num_qubits))



def build_contracted_graph_for_virt_pair(architecture, layout, nearest_free_to_comms_queues, local_distance_matrix, full_core_penalty, pair, traffic=None):
    """
    Build a contracted graph for a pair of separated virtual qubits.
    TODO - Avoid building the whole thing everytime? (performance)
    """
    virt1, virt2 = pair
    
    intercore_edges = [(e.p1, e.p2) for e in architecture.inter_core_edges]
    contracted_graph = nx.empty_graph(architecture.num_qubits)
    
    for c in range(architecture.num_cores):
        for p in architecture.core_comm_qubits[c]:
            for p_ in architecture.core_comm_qubits[c]:
                if p != p_:
                    weight = local_distance_matrix[p][p_]
                    contracted_graph.add_edge(p, p_, weight=weight)
                    
    contracted_graph.add_edges_from(intercore_edges, weight=2)
    
    # start and end edges
    phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
    core1, core2 = architecture.get_qubit_core(phys1), architecture.get_qubit_core(phys2)

    for p_comm in architecture.core_comm_qubits[core1]:
        if p_comm != phys1:
            weight = local_distance_matrix[phys1][p_comm]
            contracted_graph.add_edge(phys1, p_comm, weight=weight)
        else:
            for edge in contracted_graph.edges(phys1):
                contracted_graph.edges[edge]['weight'] += 1
        
    for p_comm in architecture.core_comm_qubits[core2]:
        if p_comm != phys2:
            weight = local_distance_matrix[phys2][p_comm]
            contracted_graph.add_edge(phys2, p_comm, weight=weight)
        else:
            for edge in contracted_graph.edges(phys2):
                contracted_graph.edges[edge]['weight'] += 1
        
    # add full core penalty and nearest free qubit penalty
    for p_comm in architecture.communication_qubits:
        for edge in contracted_graph.edges(p_comm):
            n1, n2 = edge
            core = architecture.get_qubit_core(p_comm)
            other_core = architecture.get_qubit_core(n1) if n1 != p_comm else architecture.get_qubit_core(n2)
            if core != core1 and core != core2:
                contracted_graph.edges[edge]['weight'] += (layout.get_core_capacity(core) < 2) * full_core_penalty / 2
                contracted_graph.edges[edge]['weight'] += nearest_free_to_comms_queues[p_comm].get_min_priority() / 2
            elif ((architecture.is_comm_qubit(n1) and n1 != p_comm) or (architecture.is_comm_qubit(n2) and n2 != p_comm)) and True:
                contracted_graph.edges[edge]['weight'] += (layout.get_core_capacity(core) < 2 and layout.get_core_capacity(other_core) < 2) * full_core_penalty * 100
            else:
                contracted_graph.edges[edge]['weight'] += nearest_free_to_comms_queues[p_comm].get_min_priority()
                
                
    # add penalty for gte on comm
    if architecture.is_comm_qubit(phys1):
        for edge in contracted_graph.edges(phys1):
            if architecture.get_qubit_core(edge[1]) != architecture.get_qubit_core(edge[0]):
                contracted_graph.edges[edge]['weight'] += 1
    if architecture.is_comm_qubit(phys2):
        for edge in contracted_graph.edges(phys2):
            if architecture.get_qubit_core(edge[1]) != architecture.get_qubit_core(edge[0]):
                contracted_graph.edges[edge]['weight'] += 1

    # add traffic
    if traffic is not None:
        for edge, weight in traffic.items():
            contracted_graph.edges[edge]['weight'] += weight

    return contracted_graph



def run_telesabre(config, circuit, architecture, seed=42, max_iterations=None):
    np.random.seed(seed)
    
    # Initialize metrics
    swap_count = 0
    teleportation_count = 0
    telegate_count = 0
    circuit_depth = 0
    
    # Create mapping from DAG nodes to gates
    node_to_gate = {node: circuit.gates[node] for node in circuit.dag.nodes}
    
    # Topological generations for debugging
    print("Circuit layers:", list(nx.topological_generations(circuit.dag)))
    
    # Initialize layout
    layout = initial_layout(config, circuit, architecture)
    first_layout = deepcopy(layout)
    
    # Create coupling map (undirected graph for connectivity checks)
    basic_edges = [(e.p1, e.p2) for e in architecture.edges]
    
    coupling_graph = nx.empty_graph(architecture.num_qubits)
    coupling_graph.add_edges_from(basic_edges)
    
    
    # Intra-core distance
    local_distance_matrix = nx.floyd_warshall_numpy(coupling_graph, nodelist=range(architecture.num_qubits))
    
    # Calculate a2a teleport paths
    flat_graph = nx.empty_graph(architecture.num_qubits)
    intercore_edges = [(e.p1, e.p2) for e in architecture.inter_core_edges]
    flat_graph.add_edges_from(basic_edges + intercore_edges)

    # Communication qubits nearest free qubits
    nearest_free_to_comms_queues = {}
    for p in architecture.communication_qubits:
        nearest_free_to_comms_queues[p] = SparseBucketPriorityQueue()
        core = architecture.get_qubit_core(p)
        free_qubits = layout.get_free_qubits()
        free_qubits_in_core = set(free_qubits) & set(architecture.core_qubits[core])
        for free_p in free_qubits_in_core:
            nearest_free_to_comms_queues[p].add_or_update(free_p, local_distance_matrix[p][free_p])
    
    # Other data for visualization
    arch_pos = nx.nx_pydot.graphviz_layout(flat_graph)
    arch_pos = nx.kamada_kawai_layout(flat_graph, pos=arch_pos)
    circuit_pos = nx.multipartite_layout(circuit.dag, subset_key="layer")
    arch_data = {
        'edges': [(e.p1, e.p2) for e in architecture.edges],
        'teleport_edges': [(e.p1, e.p2) for e in architecture.inter_core_edges],
        'comm_qubits': [q for q in range(architecture.num_qubits) if architecture.is_comm_qubit(q)],
        'source_qubits': list(set([e.p_source for e in architecture.teleport_edges])),
        'num_qubits': architecture.num_qubits,
        'node_positions': [arch_pos[node] for node in range(architecture.num_qubits)]
    }
    circuit_data = {
        'num_qubits': circuit.num_qubits,
        'num_gates': circuit.num_gates,
        'gates': [gate.target_qubits for gate in circuit.gates],
        'dag': [(u, v) for u, v in circuit.dag.edges],
        'node_positions': [circuit_pos[node] for node in range(circuit.num_gates)]
    }
    
    
    # Stitch the DAG for SABRE-like initial layout
    
    dag_for = circuit.dag.copy()
    for n in dag_for.nodes():
        dag_for.nodes[n]["gate"] = node_to_gate[n]


    dag_rev = nx.reverse(dag_for)
    dag_rev = nx.relabel_nodes(dag_rev, {n: f"__{n}" for n in dag_rev.nodes()})

    dag = nx.compose(dag_for, dag_rev)

    for n in list(nx.topological_generations(dag_for))[-1]:
        dag = nx.contracted_nodes(dag, n, f"__{n}")  
        
    dag_for2 = nx.relabel_nodes(dag_for, {n: f"_{n}" for n in dag_for.nodes()})
    dag = nx.compose(dag, dag_for2)

    for n in list(nx.topological_generations(dag_for2))[0]:
        dag = nx.contracted_nodes(dag, n, f"_{n}")
        
    dag = nx.convert_node_labels_to_integers(dag, label_attribute="old_label")

    node_to_gate = {n: dag.nodes[n]["gate"] for n in dag.nodes()}
    
    
    passes = [0,1,2] if config.optimize_initial else [2]
    for a in passes:
        if a == 0:
            print("Optimizing initial layout...")
            print("=== First pass (forward) ===")
        elif a == 1:
            print("=== Second pass (backward) ===")
        elif a == 2:
            print("=== Third pass (forward) ===")
            first_layout = deepcopy(layout)
            swap_count = 0
            teleportation_count = 0
            telegate_count = 0
            circuit_depth = 0
            
        # Main SABRE algorithm
        dag = circuit.dag.copy()  # Work with a copy of the DAG
        if a == 1:
            dag = dag.reverse()
        
        # Initialize frontier with nodes that have no dependencies
        front = [node for node in dag.nodes if dag.in_degree(node) == 0]
        
        # Initialize decay factors to avoid repeated swaps on the same qubits
        decay_factors = [1.0] * architecture.num_qubits
        
        # Counter for periodic decay factor reset
        decay_reset_counter = config.decay_reset # 5
        full_core_penalty = config.full_core_penalty
        
        # Main loop
        iteration = 0
        operations = []
        
        iterations_since_progress = 0
        solving_deadlock = False
        solved_deadlocks = 0
        last_progress_layout = deepcopy(layout)
        last_progress_operations = []
        last_nearest_free_to_comms_queues = deepcopy(nearest_free_to_comms_queues)
        
        last_op = None
        
        iterations_data = []
        
        while front and (max_iterations is None or iteration < max_iterations):
            try:
                if iterations_since_progress > config.safety_valve_iters and not solving_deadlock:
                    layout = last_progress_layout
                    operations = last_progress_operations
                    nearest_free_to_comms_queues = last_nearest_free_to_comms_queues
                    solving_deadlock = True
                
                # copy previous img
                #os.system(f"cp images/layout_iter.png images/layout_iter_prev.png")
                #plot_iteration(layout, architecture, circuit, f"images/layout_iter.png")
                executed_gates = []
                executed_ops = []
                executed_gates_nodes = []
                
                candidate_swaps = []
                candidate_teleports = []
                candidate_telegates_nodes = []
                candidate_telegates = []
                
                scores = []
                front_scores = []
                future_scores = []
                
                print(f"Pass: {a:02d} - Iteration {iteration:03d}")
                
                # Try to execute gates in the frontier that can be executed with current layout
                execute_gate_list = []
                for node in front:
                    gate = node_to_gate[node]
                    if layout.can_execute_gate(gate, coupling_graph):
                        execute_gate_list.append(node)
            
                needed_paths = []
                if execute_gate_list:
                    # Execute gates that can be executed
                    for node in execute_gate_list:
                        # Remove the node
                        dag.remove_node(node)
                        
                        gate = node_to_gate[node]
                        if gate.is_two_qubit():
                            executed_gates.append([layout.get_phys(q) for q in gate.target_qubits])
                            executed_gates_nodes.append(node)
                        operations.append(('gate', *gate.target_qubits))
                    print("  Executed", len(execute_gate_list), "gates.")
                    iterations_since_progress = 0  
                    
                    if solving_deadlock:
                        solved_deadlocks += 1
                        solving_deadlock = False
                        
                    last_progress_layout = deepcopy(layout)
                    last_progress_operations = deepcopy(operations)
                    last_nearest_free_to_comms_queues = deepcopy(nearest_free_to_comms_queues)
                else:
                    # No gates can be executed, need to perform a movement operation
                    
                    # === Handle separated gates ===
                
                    # 1. Get gates with virt qubits in different cores
                    separated_pairs, separated_nodes = get_separated_virt_pairs(front, node_to_gate, layout) # virt
                    
                    # Build contracted communication graph for each pair of separated gates
                    shortest_paths = []
                    for i, virts in enumerate(separated_pairs):
                        contracted_graph_g = build_contracted_graph_for_virt_pair(architecture, layout, nearest_free_to_comms_queues, local_distance_matrix, full_core_penalty, virts)
                        shortest_path = nx.shortest_path(contracted_graph_g, layout.get_phys(virts[0]), layout.get_phys(virts[1]), weight='weight')
                        shortest_paths.append(shortest_path)
                                            
                    # 2. Find shortest core path between qubits and the communication qubits in the path
                    # 3. Find nearest communication qubits to virt 1 and virt 2 and all the communication qubits in the path
                    needed_comm_qubits = [] # phys
                    needed_paths = []
                    for i, (virt1, virt2) in enumerate(separated_pairs):
                        phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                        needed_paths.append(shortest_paths[i])
                        needed_comm_qubits_i = [p for p in shortest_paths[i] if architecture.is_comm_qubit(p)]
                        needed_comm_qubits.extend(needed_comm_qubits_i)
                    
                    # 4. Find the nearest free qubit to the communication qubits
                    #nearest_free_to_comms, nearest_free_to_comms_distances = get_nearest_free_qubits(layout, local_distance_matrix, needed_comm_qubits)
                    nearest_free_to_comms = [nearest_free_to_comms_queues[p_comm].get_min() for p_comm in needed_comm_qubits]
                    
                    # 5. Add distances of the free qubits to the communications and the virt1 and virt2 to mediator in the heuristic score
                    # già fatto perchè la dist_matrix tiene conto
                    
                    # check if telegate or teleport is possible and choose the best one in terms of energy
                    for i, (virt1, virt2) in enumerate(separated_pairs):
                        phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                        path = shortest_paths[i]
                        if len(path) == 4:
                            phys_g1, phys_m1, phys_m2, phys_g2 = path
                            assert phys1 == phys_g1 and phys2 == phys_g2
                            if layout.is_phys_free(phys_m1) and layout.is_phys_free(phys_m2) and \
                                architecture.is_comm_qubit(phys_m1) and architecture.is_comm_qubit(phys_m2) and \
                                    coupling_graph.has_edge(phys1, phys_m1) and coupling_graph.has_edge(phys_m2, phys2):
                                candidate_telegates.append((phys_g1, phys_m1, phys_m2, phys_g2))
                                candidate_telegates_nodes.append(separated_nodes[i])
                    
                        needed_comm_qubits_g = [p for p in path if architecture.is_comm_qubit(p)]
                        phys_fwd_med, phys_fwd_tgt = needed_comm_qubits_g[0], needed_comm_qubits_g[1]
                        if path[0] == phys1 and path[1] == phys_fwd_med and layout.is_phys_free(phys_fwd_med) and coupling_graph.has_edge(path[0], path[1]) and \
                            layout.is_phys_free(phys_fwd_tgt) and layout.get_core_capacity(architecture.get_qubit_core(phys_fwd_tgt)) >= 2:
                            candidate_teleports.append((phys1, phys_fwd_med, phys_fwd_tgt))
                        phys_bwd_med, phys_bwd_tgt = needed_comm_qubits_g[-1], needed_comm_qubits_g[-2]
                        if path[-1] == phys2 and path[-2] == phys_bwd_med and layout.is_phys_free(phys_bwd_med) and coupling_graph.has_edge(path[-1], path[-2]) and \
                            layout.is_phys_free(phys_bwd_tgt) and layout.get_core_capacity(architecture.get_qubit_core(phys_bwd_tgt)) >= 2:
                            candidate_teleports.append((phys2, phys_bwd_med, phys_bwd_tgt))
                                
                    # === End separated gates ===
                    
                    # Candidate Swaps are swaps involving a phys qubit in the frontier or nearest free to comm qubits
                    front_phys = {layout.get_phys(virt) for node in front for virt in node_to_gate[node].target_qubits}
                    candidate_swaps = list(edge for edge in coupling_graph.edges(set(nearest_free_to_comms).union(front_phys)) if not layout.is_phys_free(edge[0]) or not layout.is_phys_free(edge[1]))
                    
                    # Calculate scores for each candidate swap
                    for i, swap in enumerate(candidate_swaps):
                        temp_layout = deepcopy(layout)
                        temp_layout.swap(*swap)
                        
                        temp_nearest_free_to_comms_queues = deepcopy(nearest_free_to_comms_queues)
                        phys1, phys2 = swap      
                        core = architecture.get_qubit_core(phys1) # should be same core phys2
                        for p_comm in architecture.core_comm_qubits[core]:
                            if temp_layout.is_phys_free(phys1):
                                temp_nearest_free_to_comms_queues[p_comm].add_or_update(phys1, local_distance_matrix[p_comm][phys1])
                            else:
                                temp_nearest_free_to_comms_queues[p_comm].remove_item(phys1)
                            if temp_layout.is_phys_free(phys2):
                                temp_nearest_free_to_comms_queues[p_comm].add_or_update(phys2, local_distance_matrix[p_comm][phys2])
                            else:
                                temp_nearest_free_to_comms_queues[p_comm].remove_item(phys2)
                        
                        decay = max(decay_factors[p] for p in swap)
                        score, front_energy, future_energy = calculate_energy(config, dag, architecture, temp_layout, temp_nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock)
                        scores.append(score)
                        front_scores.append(front_energy)
                        future_scores.append(future_energy)
                    print(f"   Swap scores: {list(map(lambda x: float(round(x,2)), scores))}")
                    
                    # Calculate scores for each candidate teleport
                    # TODO - We should save the results of hypotetical op execution and use that after decision (for performance)
                    for i, teleport in enumerate(candidate_teleports):
                        temp_layout = deepcopy(layout)
                        temp_layout.teleport(*teleport)
                        
                        temp_nearest_free_to_comms_queues = deepcopy(nearest_free_to_comms_queues)
                        phys_source, phys_mediator, phys_target = teleport
                        core_target = architecture.get_qubit_core(phys_target)
                        for p_comm in architecture.core_comm_qubits[core_target]:
                            temp_nearest_free_to_comms_queues[p_comm].remove_item(phys_target)
                        core_source = architecture.get_qubit_core(phys_source)
                        for p_comm in architecture.core_comm_qubits[core_source]:
                            temp_nearest_free_to_comms_queues[p_comm].add_or_update(phys_source, local_distance_matrix[p_comm][phys_source])
                            
                        decay = max(decay_factors[p] for p in teleport)
                        score, front_energy, future_energy = calculate_energy(config, dag, architecture, temp_layout, temp_nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock)
                        scores.append(score - config.teleport_bonus)
                        front_scores.append(front_energy)
                        future_scores.append(future_energy)
                        
                    if candidate_teleports:
                        print(f"    Teleport scores: {list(map(lambda x: float(round(x,2)), scores[-len(candidate_teleports):]))}")
                    
                    # Current score (apply telegate if any)
                    if candidate_telegates:
                        decay = 1 # capiamo
                        decay = max(decay_factors[p] for p in candidate_telegates[0])
                        score, front_energy, future_energy = calculate_energy(config, dag, architecture, temp_layout, temp_nearest_free_to_comms_queues, decay, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock)
                        scores.append(score - config.telegate_bonus)
                        front_scores.append(front_energy)
                        future_scores.append(future_energy)
                        print(f"    Telegate score: {scores[-1]:.2f}")
                    
                    # Find best swap (lowest score)
                    if not scores:
                        print("    Warning: No candidate swaps found!")
                        break
                        
                    best_op_indices = np.where(np.isclose(scores, np.min(scores)))[0]
                    best_op_idx = np.random.choice(best_op_indices)
                    
                    # Apply Swap
                    if best_op_idx < len(candidate_swaps):
                        phys1, phys2 = candidate_swaps[best_op_idx]
                        assert coupling_graph.has_edge(phys1, phys2)
                        layout.swap(phys1, phys2)
                        decay_factors[phys1] += config.swap_decay
                        decay_factors[phys2] += config.swap_decay
                        swap_count += 1
                        last_op = (phys1, phys2)
                        # Update SparseBucketPriorityQueues
                        core = architecture.get_qubit_core(phys1) # should be same core phys2
                        for p_comm in architecture.core_comm_qubits[core]:
                            if layout.is_phys_free(phys1):
                                nearest_free_to_comms_queues[p_comm].add_or_update(phys1, local_distance_matrix[p_comm][phys1])
                            else:
                                nearest_free_to_comms_queues[p_comm].remove_item(phys1)
                            if layout.is_phys_free(phys2):
                                nearest_free_to_comms_queues[p_comm].add_or_update(phys2, local_distance_matrix[p_comm][phys2])
                            else:
                                nearest_free_to_comms_queues[p_comm].remove_item(phys2)
                            #print("p_comm", p_comm, "phys1", phys1, "phys2", phys2, "dist1", local_distance_matrix[p_comm][phys1], "dist2", local_distance_matrix[p_comm][phys2], "min dist", nearest_free_to_comms_queues[p_comm].get_min_priority())
                    # Apply Teleport
                    elif best_op_idx < len(candidate_swaps) + len(candidate_teleports):
                        phys_source, phys_mediator, phys_target = candidate_teleports[best_op_idx - len(candidate_swaps)]
                        layout.teleport(phys_source, phys_mediator, phys_target)
                        decay_factors[phys_source] += config.teleport_decay
                        decay_factors[phys_mediator] += config.teleport_decay
                        decay_factors[phys_target] += config.teleport_decay
                        teleportation_count += 1
                        last_op = (phys_source, phys_mediator, phys_target)
                        # Update SparseBucketPriorityQueues
                        core_target = architecture.get_qubit_core(phys_target)
                        for p_comm in architecture.core_comm_qubits[core_target]:
                            nearest_free_to_comms_queues[p_comm].remove_item(phys_target)
                        core_source = architecture.get_qubit_core(phys_source)
                        for p_comm in architecture.core_comm_qubits[core_source]:
                            nearest_free_to_comms_queues[p_comm].add_or_update(phys_source, local_distance_matrix[p_comm][phys_source])
                    # Apply Telegate
                    else:
                        for k, node in enumerate(candidate_telegates_nodes):
                            dag.remove_node(node)
                            telegate_count += 1
                            last_op = candidate_telegates[k]
                            executed_gates_nodes.append(node)
                            for p in candidate_telegates[k]:
                                decay_factors[p] += config.telegate_decay
                        print("  Applied", len(candidate_telegates), "telegates.")
                        
                    executed_ops.append(last_op)
                    operations.append(('move', *last_op))
                
                # Update frontier with nodes that have no dependencies
                front = [node for node in dag.nodes if dag.in_degree(node) == 0]
                if solving_deadlock:
                    front = front[:1]
                    print(f"  Solving deadlock considering only {[node_to_gate[node].target_qubits for node in front]}, "
                        f"Last swap: {list(map(int,last_op))}" if last_op is not None else "")
                # Debugging output
                else:
                    print(f"  Remaining nodes: {len(dag)}, Swaps: {swap_count}, " +
                        f"Front: {[node_to_gate[node].target_qubits for node in front]}, " +
                        (f"Last swap: {list(map(int,last_op))}, " if last_op is not None else "") +
                        f"Iters since progress: {iterations_since_progress}")
                
                # Periodically reset decay factors
                decay_reset_counter -= 1
                if decay_reset_counter == 0:
                    decay_reset_counter = config.decay_reset
                    decay_factors = [1.0] * architecture.num_qubits

                #plot_iteration(layout, architecture, circuit, f"images/layout_iter_{iteration:04}.png", gates=executed_gates, ops=executed_ops, dag=dag, node_to_gate=node_gate)

                candidate_ops = candidate_swaps + candidate_teleports + candidate_telegates
                # sort by score
                candidate_ops = [op for _, op in sorted(zip(scores, candidate_ops))]
                future_scores = [op for _, op in sorted(zip(scores, future_scores))]
                front_scores = [op for _, op in sorted(zip(scores, front_scores))]
                scores = sorted(scores)
                
                # expand needed paths
                expanded_paths = []
                for path in needed_paths:
                    expanded_path = []
                    for i in range(len(path)-1):
                        u, v = path[i], path[i+1]
                        core_u, core_v = architecture.get_qubit_core(u), architecture.get_qubit_core(v)
                        if architecture.is_comm_qubit(u) and architecture.is_comm_qubit(v) and core_u != core_v:
                            if i > 0:
                                expanded_path.append(v)
                            else:
                                expanded_path.extend([u,v])
                        else:
                            short = nx.shortest_path(coupling_graph, u, v)
                            if i > 0:
                                expanded_path.extend(short[1:])
                            else:
                                expanded_path.extend(short)
                    expanded_paths.append(expanded_path)
                
                iterations_data.append({
                    'phys_to_virt': layout.phys_to_virt.tolist(),
                    'virt_to_phys': layout.virt_to_phys.tolist(),
                    'swap_count': swap_count,
                    'teleportation_count': teleportation_count,
                    'telegate_count': telegate_count,
                    'remaining_nodes': list(dag.nodes),
                    'front': front,
                    'gates': executed_gates_nodes,
                    'applied_gates': executed_gates,
                    'applied_ops': executed_ops,
                    'needed_paths': expanded_paths,
                    'energy': calculate_energy(config, dag, architecture, layout, nearest_free_to_comms_queues, 1, node_to_gate, local_distance_matrix, full_core_penalty, solving_deadlock),
                    'candidate_ops': candidate_ops,
                    'candidate_ops_scores': scores,
                    'candidate_ops_front_scores': front_scores,
                    'candidate_ops_future_scores': future_scores,
                    'solving_deadlock': solving_deadlock,
                })
                
                if iteration % 10 == 0 and config.save_data:
                    with open("viewer/data.json", "w") as f:
                        json.dump({"architecture": arch_data, "circuit": circuit_data, "iterations": iterations_data}, f, cls=NpEncoder)
                                
                iteration += 1
                iterations_since_progress += 1
                
                if solving_deadlock and iterations_since_progress > config.max_solving_deadlock_iterations:
                    print("  Not able to solve deadlock, stopping...")
                    break
                
            except KeyboardInterrupt:
                print("CTRL+C detected, stopping...")
                break
                
    if config.save_data:
        with open("viewer/data.json", "w") as f:
            json.dump({"architecture": arch_data, "circuit": circuit_data, "iterations": iterations_data}, f, cls=NpEncoder)
        
    # Count Ops
    
    swap_count = 0
    teleportation_count = 0
    telegate_count = 0
    
    used_qubits = [False] * architecture.num_qubits
    for op in operations:
        if op[0] == 'move':
            if len(op[1:]) == 2 and any([used_qubits[q] for q in op[1:]]):
                swap_count += 1
                for q in op[1:]:
                    used_qubits[q] = True
            elif len(op[1:]) == 3 and any([used_qubits[q] for q in op[1:]]):
                teleportation_count += 1
                for q in op[1:]:
                    used_qubits[q] = True
            elif len(op[1:]) == 4:
                telegate_count += 1
                for q in op[1:]:
                    used_qubits[q] = True
        elif op[0] == 'gate':
            for q in op[1:]:
                used_qubits[q] = True
                
    a_swap_count = sum(1 for op in operations if op[0] == 'move' and len(op[1:]) == 2)
    a_teleportation_count = sum(1 for op in operations if op[0] == 'move' and len(op[1:]) == 3)
    a_telegate_count = sum(1 for op in operations if op[0] == 'move' and len(op[1:]) == 4)
    
    qubit_depths = [0] * architecture.num_qubits
    qubit_depths_tp = [0] * architecture.num_qubits
    for op in operations:
        max_depth = max(depth for p, depth in enumerate(qubit_depths) if p in op[1:])
        duration = 1
        if op[0] == 'move':
            if len(op[1:]) == 2:
                duration = 1
            elif len(op[1:]) == 3:
                duration = 5
            elif len(op[1:]) == 4:
                duration = 5
        for p in op[1:]:
            qubit_depths[p] = max_depth + duration
            if len(op[1:]) > 2 and op[0] == 'move':
                qubit_depths_tp[p] = max_depth + duration
    circuit_depth = max(qubit_depths)
    teleport_depth = max(qubit_depths_tp)
    
    print("\n\nFinal results:")
    print(f"  Swaps: {swap_count} ({a_swap_count})")
    print(f"  Teleports: {teleportation_count} ({a_teleportation_count})")
    print(f"  Teleport Gates: {telegate_count} ({a_telegate_count})")
    print(f"  Depth: {circuit_depth}")
    print(f"  Inter-core depth: {teleport_depth}")
    print(f"  Solved Deadlocks: {solved_deadlocks}")
    
    return swap_count, teleportation_count, telegate_count, circuit_depth, teleport_depth, solved_deadlocks, first_layout, solving_deadlock


"""

Swaps: 276
Teleports: 35
Teleport Gates: 103
Depth: 0
"""

