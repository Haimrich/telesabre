#!/usr/bin/env python3

import json
import argparse
import os

import networkx as nx
import matplotlib.pyplot as plt


def device_plot(data : dict, filename : str = "device.png"):
    name = data['name']
    num_qubits = data['num_qubits']
    intra = data['intra_core_edges']
    inter = data['inter_core_edges']
    pos = data['node_positions']

    graph = nx.Graph()
    graph.add_nodes_from(range(num_qubits))
    graph.add_edges_from(intra + inter)

    inter_nodes = set(u for edge in inter for u in edge)

    plt.figure(figsize=(8, 6))
    nx.draw_networkx_edges(graph, pos, edgelist=intra, edge_color="#1B6CDC", width=1.5)
    nx.draw_networkx_edges(graph, pos, edgelist=inter, edge_color="#FF8800", width=2.5, style="dashed")
    nx.draw_networkx_nodes(graph, pos, nodelist=[n for n in graph.nodes if n not in inter_nodes], node_color="#2B3C82", node_size=80)
    nx.draw_networkx_nodes(graph, pos, nodelist=list(inter_nodes), node_color="#E84A5F", node_size=120, edgecolors='black')
    nx.draw_networkx_labels(graph, pos, font_size=5, font_color='white')
    plt.text(0.5, 1.05, f"{name}", ha='center', fontsize=10, transform=plt.gca().transAxes, color='#1B6CDC')
    plt.axis('off')
    plt.savefig(filename, dpi=300, bbox_inches='tight', transparent=True)
    plt.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot device json configuration.")
    parser.add_argument("filename", type=str, help="Input JSON file with device configuration.")
    args = parser.parse_args()
    
    filename = args.filename
    filename_noext = os.path.splitext(filename)[0]
    
    with open(filename, "r") as f:
        data = json.load(f)
        data = data['device']
    
    device_plot(data, filename=f"{filename_noext}.png")