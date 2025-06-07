#pragma once

#include <stddef.h>

typedef int node_t;

typedef struct {
    node_t to;
    int weight;
} edge_t;

typedef struct {
    edge_t *edges;
    size_t degree;
    size_t capacity;
} adj_list_t;

typedef struct {
    node_t *nodes;
    size_t length;
    float distance;
} path_t;

typedef struct {
    adj_list_t* adj;
    size_t num_nodes;
} graph_t;


graph_t* graph_new(size_t num_nodes);

void graph_free(graph_t* graph);

void graph_add_directed_edge(graph_t* graph, int u, int v, int w);

void graph_add_edge(graph_t* graph, int u, int v, int w);

path_t* graph_dijkstra(const graph_t* graph, int src, int dst);

void graph_print(const graph_t* graph, const int *node_ids_translation);

graph_t* graph_copy(const graph_t* src);

path_t* path_copy(const path_t* src);

void path_free(path_t* path);