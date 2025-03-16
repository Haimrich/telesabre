import colorsys

import matplotlib.pyplot as plt
import matplotlib.patches as patches



def plot_iteration(layout, architecture, circuit, filename, gates=None, ops=None, dag=None, node_to_gate=None):
    px = 1.5/plt.rcParams['figure.dpi']  # pixel in inches
    fig, axes = plt.subplots(figsize=(1920*px, 1080*px), nrows=2, ncols=1)
    edges = [(e.p1, e.p2) for e in architecture.edges]

    virt_colors = generate_hex_colors(circuit.num_qubits)
    
    # Draw Remaining DAG
    if dag is not None:
        ax = axes[1]
        ax.set_title("Remaining DAG")
        pos_dag = nx.multipartite_layout(dag, subset_key="layer")
        nx.draw_networkx_edges(dag, pos_dag, ax=ax)
        for node, (x, y) in pos_dag.items():
            # Define box size
            width, height = 0.03, 0.015  # Adjust as needed
            gate = node_to_gate[node]
            
            if gate.is_two_qubit():
                virt1, virt2 = gate.target_qubits
                q1 = patches.Rectangle((x - width / 2, y), width, height, 
                                        edgecolor='black', facecolor=virt_colors[virt1], lw=1)
                q2 = patches.Rectangle((x - width / 2, y-height), width, height, 
                                        edgecolor='black', facecolor=virt_colors[virt2], lw=1)
                ax.add_patch(q1)
                ax.add_patch(q2)
                plt.text(x, y + height / 2, str(virt1), ha='center', va='center', fontsize=12)
                plt.text(x, y - height / 2, str(virt2), ha='center', va='center', fontsize=12)
            else:
                virt = gate.target_qubits[0]
                q = patches.Rectangle((x - width / 2, y - height/2), width, height, 
                                      edgecolor='black', facecolor=virt_colors[virt], lw=1)
                ax.add_patch(q)
                plt.text(x, y, str(virt), ha='center', va='center', fontsize=12)

    # Plot Current Layout and OPs
    ax = axes[0]
    ax.set_title("Current Layout")
    G = nx.Graph()

    G.add_edges_from(edges, color="gray", weight=1, style='-')

    tp_edges = [(e.p1, e.p2) for e in architecture.inter_core_edges]
    G.add_edges_from(tp_edges, color="#03045e", weight=2, style='dotted')
    
    pos = nx.kamada_kawai_layout(G)

    mediators = set(e.p_mediator for e in architecture.teleport_edges)
    sources = set(e.p_source for e in architecture.teleport_edges)

    mapping = layout.phys_to_virt
    
    if gates is not None:
        G.add_edges_from(gates, color="green", weight=2, style='dotted')
        
    if ops is not None:
        for i, op in enumerate(ops):
            if len(op) == 2: # swap
                G.add_edge(op[0], op[1], color="blue", weight=2, style='dotted')
            elif len(op) == 3: # teleport
                G.add_edge(op[0], op[1], color="red", weight=2, style='dotted')
                G.add_edge(op[1], op[2], color="red", weight=2, style='dotted')
            elif len(op) == 4: # telegate
                G.add_edge(op[0], op[1], color="purple", weight=2, style='dotted')
                G.add_edge(op[1], op[2], color="purple", weight=2, style='dotted')
                G.add_edge(op[2], op[3], color="purple", weight=2, style='dotted')
        
    for i, node in enumerate(G.nodes):
        G.nodes[node]['label'] = f"{node}: {mapping[node]}" if mapping[node] <= circuit.num_qubits else f"{node}: -"
        G.nodes[node]['color'] = virt_colors[mapping[node]] if mapping[node] < circuit.num_qubits else '#c9c9c9'
        
        if node in mediators:
            G.nodes[node]['edgecolor'] = '#6a3a87'
            G.nodes[node]['marker'] = "H"
        elif node in sources:
            G.nodes[node]['edgecolor'] = '#d43fcc'
            G.nodes[node]['marker'] = "o"
        else:
            G.nodes[node]['edgecolor'] = '#ffffff'
            G.nodes[node]['marker'] = "o"
                
    node_colors = nx.get_node_attributes(G,'color').values()
    node_edge_colors = nx.get_node_attributes(G,'edgecolor').values()
    edge_colors = nx.get_edge_attributes(G,'color').values()
    labels = nx.get_node_attributes(G, "label") 

    _ = nx.draw_networkx_nodes(G, pos, ax=ax, node_size=600, node_color=node_colors, edgecolors=node_edge_colors, node_shape='o')
    _ = nx.draw_networkx_labels(G, pos, font_size=7, ax=ax, labels=labels, font_weight="bold")
    _ = nx.draw_networkx_edges(G, pos, edge_color=edge_colors, ax=ax, node_size=600, width=3)
    
    fig.savefig(filename, bbox_inches='tight')



def generate_hex_colors(n, saturation=0.8, lightness=0.8):
    colors = []
    for i in range(n):
        hue = i / n  # Distribute hues evenly between 0 and 1
        r, g, b = colorsys.hls_to_rgb(hue, lightness, saturation)
        hex_color = f'#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}'
        colors.append(hex_color)
    return colors

