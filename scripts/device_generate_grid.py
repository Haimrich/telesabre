#!/usr/bin/env python3

import json
import argparse
import os

def grid_edges(nrows, ncols):
    return [(i*ncols+j, i*ncols+j+1) for i in range(nrows) for j in range(ncols-1)] + \
           [(i*ncols+j, (i+1)*ncols+j) for i in range(nrows-1) for j in range(ncols)]

def make_grid_grid_device(cores_xy, cores_y, qubits_x, qubits_y):
    intra, inter = [], []
    nodes_per_core = qubits_x * qubits_y
    for r in range(cores_xy):
        for c in range(cores_y):
            base = (r*cores_y+c)*nodes_per_core
            intra += [(base+u, base+v) for u, v in grid_edges(qubits_x, qubits_y)]
            # right neighbor (horizontal)
            if c+1 < cores_y:
                i = qubits_x // 2
                a = base + i*qubits_y + (qubits_y-1)
                b = base + nodes_per_core + i*qubits_y + 0
                inter.append((a, b))
            # bottom neighbor (vertical)
            if r+1 < cores_xy:
                j = qubits_y // 2
                a = base + (qubits_x-1)*qubits_y + j
                b = base + cores_y*nodes_per_core + j
                inter.append((a, b))
    return intra, inter

def make_regular_layout(cores_x, cores_y, qubits_x, qubits_y, spacing=1.5):
    pos = {}
    nodes_per_core = qubits_x * qubits_y
    for r in range(cores_x):
        for c in range(cores_y):
            core_base = (r * cores_y + c) * nodes_per_core
            for i in range(qubits_x):
                for j in range(qubits_y):
                    node = core_base + i * qubits_y + j
                    # Position: (core_x offset + intra_x, core_y offset + intra_y)
                    x = c * spacing * (qubits_y - 1) + j
                    y = r * spacing * (qubits_x - 1) + i
                    pos[node] = (x, -y)  # flip y for drawing top-to-bottom
    return pos

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a grid device configuration.")
    parser.add_argument("-n", "--name", type=str, default="grid_device", help="Name of the device.")
    parser.add_argument("-cx", "--cores_x", type=int, default=2, help="Number of cores in the x direction.")
    parser.add_argument("-cy", "--cores_y", type=int, default=3, help="Number of cores in the y direction.")
    parser.add_argument("-qx", "--qubits_x", type=int, default=2, help="Number of qubits in the x direction per core.")
    parser.add_argument("-qy", "--qubits_y", type=int, default=4, help="Number of qubits in the y direction per core.")
    parser.add_argument("-o", "--output", type=str, default="grid_device", help="Output directory for the device configuration.")
    parser.add_argument("-p", "--plot", action="store_true", help="Generate a plot of the device configuration.")
    args = parser.parse_args()

    name = args.name
    num_cores = args.cores_x * args.cores_y
    num_qubits = num_cores * args.qubits_x * args.qubits_y
    intra, inter = make_grid_grid_device(
        cores_xy=args.cores_x, 
        cores_y=args.cores_y, 
        qubits_x=args.qubits_x, 
        qubits_y=args.qubits_y
    )
    
    pos = make_regular_layout(args.cores_x, args.cores_y, args.qubits_x, args.qubits_y)
    node_positions = [pos[node] for node in range(num_qubits)]
    
    device = {
        "name": name,
        "num_cores": num_cores,
        "num_qubits": num_qubits,
        "intra_core_edges": intra,
        "inter_core_edges": inter,
        "node_positions": node_positions
    }

    os.makedirs(args.output, exist_ok=True)
    
    with open(f"{args.output}/{args.name}.json", "w") as f:
        json.dump({"device": device}, f, indent=2)
    
    if args.plot:
        from device_plot import device_plot
        device_plot(device, filename=f"{args.output}/{args.name}.png")