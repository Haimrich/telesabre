import json

import networkx as nx
import numpy as np

from copy import deepcopy

from layout import Layout
from utils import NpEncoder, SparseBucketPriorityQueue
from plotting import plot_iteration


def get_candidate_swaps(layout, coupling_map, front, node_gate):
    # Get physical qubits used by gates in the frontier
    used_phys = {layout.get_phys(virt) for node in front for virt in node_gate[node].target_qubits}
    
    # Get all edges in the coupling map that involve used physical qubits
    candidate_swaps_phys = list(coupling_map.edges(used_phys))
      
    return candidate_swaps_phys


def get_candidate_swaps_comm_qubits(layout, coupling_map, needed_comm_qubits):
    """Get candidate swaps for needed communications qubits."""
    candidate_swaps = list(coupling_map.edges(needed_comm_qubits))
    return candidate_swaps



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



def calculate_energy(dag, hyp_layout, dist_matrix, decay, node_to_gate, interpaths, architecture, intracore_dist_matrix):
    score = 0
    
    # Calculate weighted score using exponential decay based on depth
    for depth, layer in enumerate(nx.topological_generations(dag)):
        lookahead_factor = 0.5 ** depth
        lookahead_factor = 1 if depth == 0 else 0
        for node in layer:
            if node_to_gate[node].is_two_qubit():
                virt1, virt2 = node_to_gate[node].target_qubits
                phys1, phys2 = hyp_layout.get_phys(virt1), hyp_layout.get_phys(virt2)
                distance = dist_matrix[phys1][phys2]
                score += distance * lookahead_factor  # Apply exponential decay with depth
                
        # Add distances of the free qubits to the communications and the virt1 and virt2 to mediator
        separated_pairs, _ = get_separated_virt_pairs(layer, node_to_gate, hyp_layout) # virt
        needed_comm_qubits = [] # phys
        for i, (virt1, virt2) in enumerate(separated_pairs):
            phys1, phys2 = hyp_layout.get_phys(virt1), hyp_layout.get_phys(virt2)
            path = interpaths[phys1][phys2]
            needed_comm_qubits_i = [p for p in path if architecture.is_comm_qubit(p)]
            needed_comm_qubits.extend(needed_comm_qubits_i)
        _, nearest_free_to_comms_distances = get_nearest_free_qubits(hyp_layout, intracore_dist_matrix, needed_comm_qubits)
        score += sum(nearest_free_to_comms_distances) * lookahead_factor * 1.01
    
    # Apply decay factor to score
    score *= decay
                
    return score


def initial_layout(circuit, architecture):
    """Builds naive initial layout that allows teleports (no core with capacity < 2)."""
    core_capacities = [architecture.num_qubits // architecture.num_cores] * architecture.num_cores
    phys_to_virt = []
    virt = 0
    virt_empty = circuit.num_qubits
    for i in range(architecture.num_qubits):
        core = architecture.get_qubit_core(i)
        if core_capacities[core] > 1 and virt < circuit.num_qubits:
            core_capacities[core] -= 1
            phys_to_virt.append(virt)
            virt += 1
        else:
            phys_to_virt.append(virt_empty)
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


def sabre_mapping(circuit, architecture, seed=42, max_iterations=None):
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
    layout = initial_layout(circuit, architecture)
    
    # Create coupling map (undirected graph for connectivity checks)
    basic_edges = [(e.p1, e.p2) for e in architecture.edges]
    
    coupling_graph = nx.empty_graph(architecture.num_qubits)
    coupling_graph.add_edges_from(basic_edges)
    
    #  Distance Graph
    global_distance_matrix = calculate_global_distance_matrix(architecture)
    
    # Intra-core distance
    local_distance_matrix = nx.floyd_warshall_numpy(coupling_graph, nodelist=range(architecture.num_qubits))
    
    # Calculate a2a teleport paths
    flat_graph = nx.empty_graph(architecture.num_qubits)
    intercore_edges = [(e.p1, e.p2) for e in architecture.inter_core_edges]
    flat_graph.add_edges_from(basic_edges + intercore_edges)

    interpaths = dict(nx.all_pairs_shortest_path(flat_graph)) 
    
    # Contracted graph for communication qubits
    contracted_graph = nx.empty_graph(architecture.num_qubits)
    for c in range(architecture.num_cores):
        for p in architecture.core_comm_qubits[c]:
            for p_ in architecture.core_comm_qubits[c]:
                if p != p_:
                    contracted_graph.add_edge(p, p_, weight=local_distance_matrix[p][p_])
    contracted_graph.add_edges_from(intercore_edges, weight=1)
    
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
    arch_pos = nx.kamada_kawai_layout(flat_graph, pos=nx.planar_layout(flat_graph))
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
    
    
    # Main SABRE algorithm
    dag = circuit.dag.copy()  # Work with a copy of the DAG
    
    # Initialize frontier with nodes that have no dependencies
    front = [node for node in dag.nodes if dag.in_degree(node) == 0]
    
    # Initialize decay factors to avoid repeated swaps on the same qubits
    decay_factors = [1.0] * architecture.num_qubits
    
    # Counter for periodic decay factor reset
    reset_counter = 5
    
    
    # Main loop
    iteration = 0
    last_op = None
    
    iterations_data = []
    
    #max_iterations = 10
    while front and (max_iterations is None or iteration < max_iterations):
        try:
            # copy previous img
            #os.system(f"cp images/layout_iter.png images/layout_iter_prev.png")
            #plot_iteration(layout, architecture, circuit, f"images/layout_iter.png")
            executed_gates = []
            executed_ops = []
            executed_gates_nodes = []
            
            print("Iteration", iteration)
            iteration += 1
            
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
                print("  Executed", len(execute_gate_list), "gates.")
            else:
                # No gates can be executed, need to perform a movement operation
                
                
                # SEPARATED GATES
            
                # 1. Get gates with virt qubits in different cores
                separated_pairs, separated_nodes = get_separated_virt_pairs(front, node_to_gate, layout) # virt
                
                # 2. Find shortest core path between qubits and the communication qubits in the path
                # 3. Find nearest communication qubits to virt 1 and virt 2 and all the communication qubits in the path
                needed_comm_qubits = [] # phys
                needed_paths = []
                for i, (virt1, virt2) in enumerate(separated_pairs):
                    phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                    path = interpaths[phys1][phys2]
                    needed_paths.append(path)
                    needed_comm_qubits_i = [p for p in path if architecture.is_comm_qubit(p)]
                    needed_comm_qubits.extend(needed_comm_qubits_i)
                
                # 4. Find the nearest free qubit to the communication qubits
                nearest_free_to_comms, nearest_free_to_comms_distances = get_nearest_free_qubits(layout, local_distance_matrix, needed_comm_qubits)
                print("   separated_pairs", separated_pairs)
                print("   nearest free distances", nearest_free_to_comms_distances)
                print("   needed paths", needed_paths)
                print("   needed comm", needed_comm_qubits)
                
                # 5. Add distances of the free qubits to the communications and the virt1 and virt2 to mediator in the heuristic score
                # già fatto perchè la dist_matrix tiene conto
                
                # check if telegate or teleport is possible and choose the best one in terms of heuristic score
                candidate_telegates = []
                candidate_telegates_nodes = []
                candidate_teleports = []
                for i, (virt1, virt2) in enumerate(separated_pairs):
                    phys1, phys2 = layout.get_phys(virt1), layout.get_phys(virt2)
                    path = interpaths[phys1][phys2]
                    if len(path) == 4:
                        phys_g1, phys_m1, phys_m2, phys_g2 = path
                        assert phys1 == phys_g1 and phys2 == phys_g2
                        if layout.is_phys_free(phys_m1) and layout.is_phys_free(phys_m2) and \
                            architecture.is_comm_qubit(phys_m1) and architecture.is_comm_qubit(phys_m2):
                            candidate_telegates.append((phys_g1, phys_m1, phys_m2, phys_g2))
                            candidate_telegates_nodes.append(separated_nodes[i])
                    else:
                        needed_comm_qubits_g = [p for p in path if architecture.is_comm_qubit(p)]
                        nearest_free_to_comms_g, _ = get_nearest_free_qubits(layout, local_distance_matrix, needed_comm_qubits_g)
                        phys_fwd_med, phys_fwd_tgt = nearest_free_to_comms_g[0], nearest_free_to_comms_g[1]
                        if path[0] == phys1 and path[1] == phys_fwd_med and layout.is_phys_free(phys_fwd_med) and \
                            layout.is_phys_free(phys_fwd_tgt) and layout.get_core_capacity(architecture.get_qubit_core(phys_fwd_tgt)) >= 2 and \
                                architecture.is_comm_qubit(phys_fwd_tgt) and architecture.is_comm_qubit(phys_fwd_med):
                            candidate_teleports.append((phys1, phys_fwd_med, phys_fwd_tgt))
                        phys_bwd_med, phys_bwd_tgt = nearest_free_to_comms_g[-1], nearest_free_to_comms_g[-2]
                        if path[-1] == phys2 and path[-2] == phys_bwd_med and layout.is_phys_free(phys_bwd_med) and \
                            layout.is_phys_free(phys_bwd_tgt) and layout.get_core_capacity(architecture.get_qubit_core(phys_bwd_tgt)) >= 2 and \
                                architecture.is_comm_qubit(phys_bwd_tgt) and architecture.is_comm_qubit(phys_bwd_med):
                            candidate_teleports.append((phys2, phys_bwd_med, phys_bwd_tgt))
                            
                # end SEPARATED GATES
                
                candidate_swaps = get_candidate_swaps(layout, coupling_graph, front, node_to_gate)
                candidate_swaps += get_candidate_swaps_comm_qubits(layout, coupling_graph, nearest_free_to_comms)
                
                scores = []
                
                # Calculate scores for each candidate swap
                for i, swap in enumerate(candidate_swaps):
                    temp_layout = deepcopy(layout)
                    temp_layout.swap(*swap)
                    decay = max(decay_factors[p] for p in swap)
                    score = calculate_energy(dag, temp_layout, global_distance_matrix, decay, node_to_gate, interpaths, architecture, local_distance_matrix)
                    scores.append(score)
                print(f"   Swap scores: {list(map(lambda x: float(round(x,2)), scores))}")
                
                # Calculate scores for each candidate teleport
                for i, teleport in enumerate(candidate_teleports):
                    temp_layout = deepcopy(layout)
                    temp_layout.teleport(*teleport)
                    decay = max(decay_factors[p] for p in teleport)
                    score = calculate_energy(dag, temp_layout, global_distance_matrix, decay, node_to_gate, interpaths, architecture, local_distance_matrix) - 100
                    scores.append(score)
                if candidate_teleports:
                    print(f"    Teleport scores: {list(map(lambda x: float(round(x,2)), scores[-len(candidate_teleports):]))}")
                
                # Current score (apply telegate if any)
                if candidate_telegates:
                    scores.append(calculate_energy(dag, layout, global_distance_matrix, 1, node_to_gate, interpaths, architecture, local_distance_matrix)- 1000)
                    print(f"    Telegate score: {scores[-1]:.2f}")
                
                # Find best swap (lowest score)
                if not scores:
                    print("    Warning: No candidate swaps found!")
                    break
                    
                best_op_indices = np.where(np.isclose(scores, np.min(scores)))[0]
                best_op_idx = np.random.choice(best_op_indices)
                
                if best_op_idx < len(candidate_swaps):
                    phys1, phys2 = candidate_swaps[best_op_idx]
                    assert coupling_graph.has_edge(phys1, phys2)
                    layout.swap(phys1, phys2)
                    decay_factors[phys1] += 0.001
                    decay_factors[phys2] += 0.001
                    swap_count += 1
                    last_op = (phys1, phys2)
                elif best_op_idx < len(candidate_swaps) + len(candidate_teleports):
                    phys_source, phys_mediator, phys_target = candidate_teleports[best_op_idx - len(candidate_swaps)]
                    layout.teleport(phys_source, phys_mediator, phys_target)
                    decay_factors[phys_source] += 0.001
                    decay_factors[phys_mediator] += 0.001
                    decay_factors[phys_target] += 0.001
                    teleportation_count += 1
                    last_op = (phys_source, phys_mediator, phys_target)
                else:
                    for k, node in enumerate(candidate_telegates_nodes):
                        dag.remove_node(node)
                        telegate_count += 1
                        last_op = candidate_telegates[k]
                        executed_gates_nodes.append(node)
                    print("  Applied", len(candidate_telegates), "telegates.")
                executed_ops.append(last_op)
                
            # Update frontier with nodes that have no dependencies
            front = [node for node in dag.nodes if dag.in_degree(node) == 0]
            
            # Debugging output
            print(f"  Remaining nodes: {len(dag)}, Swaps: {swap_count}, "
                f"Front: {[node_to_gate[node].target_qubits for node in front]}, "
                f"Last swap: {list(map(int,last_op))}" if last_op is not None else "")
            
            # Periodically reset decay factors
            reset_counter -= 1
            if reset_counter == 0 or True:
                reset_counter = 5
                decay_factors = [1.0] * architecture.num_qubits

            #plot_iteration(layout, architecture, circuit, f"images/layout_iter_{iteration:04}.png", gates=executed_gates, ops=executed_ops, dag=dag, node_to_gate=node_gate)
        
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
                'needed_paths': needed_paths,
                'energy': calculate_energy(dag, layout, global_distance_matrix, 1, node_to_gate, interpaths, architecture, local_distance_matrix)
            })
            
            if iteration % 10 == 0:
                with open("viewer/data.json", "w") as f:
                    json.dump({"architecture": arch_data, "circuit": circuit_data, "iterations": iterations_data}, f, cls=NpEncoder)
        except KeyboardInterrupt:
            print("CTRL+C detected, stopping...")
            break
                
    with open("viewer/data.json", "w") as f:
        json.dump({"architecture": arch_data, "circuit": circuit_data, "iterations": iterations_data}, f, cls=NpEncoder)
    return swap_count, teleportation_count, telegate_count, circuit_depth

