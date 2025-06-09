#include "graph.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "heap.h"
#include "utils.h"


static void adj_list_add(adj_list_t *list, int to, int weight) {
    if (list->degree == list->capacity) {
        size_t new_cap = list->capacity * 2;
        list->edges = realloc(list->edges, sizeof(edge_t) * new_cap);
        list->capacity = new_cap;
    }
    list->edges[list->degree].to = to;
    list->edges[list->degree].weight = weight;
    list->degree++;
}


void graph_add_directed_edge(graph_t *graph, int u, int v, int w) {
    adj_list_add(&graph->adj[u], v, w);
}


void graph_add_edge(graph_t *graph, int u, int v, int w) {
    graph_add_directed_edge(graph, u, v, w);
    if (u != v) graph_add_directed_edge(graph, v, u, w);
}


void graph_increase_edge_weight(graph_t *graph, int u, int v, int w) {
    for (size_t i = 0; i < graph->adj[u].degree; ++i) {
        if (graph->adj[u].edges[i].to == v) {
            graph->adj[u].edges[i].weight += w;
            return;
        }
    }
}


void graph_set_node_weight(graph_t *graph, int node, int weight) {
    graph->node_weights[node] = weight;
}


void graph_increase_node_weight(graph_t *graph, int node, int weight) {
    graph->node_weights[node] += weight;
}


path_t *graph_dijkstra(const graph_t *graph, int src, int dst) {
    size_t N = graph->num_nodes;
    int *dist = malloc(sizeof(int) * N);
    int *prev = malloc(sizeof(int) * N);
    bool *visited = calloc(N, sizeof(bool));
    check_alloc(3, dist, prev, visited);

    for (size_t i = 0; i < N; ++i) {
        dist[i] = TS_INF;
        prev[i] = -1;
    }
    dist[src] = graph->node_weights[src];

    heap_t *heap = heap_new(N);
    check_alloc(4, heap, dist, prev, visited);
    heap_insert(heap, src, dist[src]);

    while (!heap_is_empty(heap)) {
        heap_item_t min = heap_extract_min(heap);
        int u = min.id;
        if (visited[u]) continue;
        visited[u] = true;
        if (u == dst) break;
        adj_list_t *adj = &graph->adj[u];
        for (size_t i = 0; i < adj->degree; ++i) {
            int v = adj->edges[i].to;
            int weight = adj->edges[i].weight;
            if (visited[v]) continue;
            int node_weight = graph->node_weights[v];
            if (dist[u] != TS_INF && dist[u] + weight + node_weight < dist[v]) {
                dist[v] = dist[u] + weight + node_weight;
                prev[v] = u;
                heap_insert(heap, v, dist[v]);
            }
        }
    }

    path_t *result = malloc(sizeof(path_t));
    check_alloc(4, result, dist, prev, visited);

    if (dist[dst] != TS_INF) {
        size_t len = 0;
        for (int cur = dst; cur != -1; cur = prev[cur]) ++len;
        result->nodes = malloc(sizeof(int) * len);
        result->distances = malloc(sizeof(int) * (len-1));
        check_alloc(5, result->nodes, result, dist, prev, visited);
        int cur = dst;
        for (size_t i = len; i > 0; i--) {
            result->nodes[i - 1] = cur;
            if (i != 1) {
                result->distances[i - 2] = dist[cur] - dist[prev[cur]];
            }
            cur = prev[cur];
        }

        
        result->length = len;
        result->distance = dist[dst];
    } else {
        result->nodes = NULL;
        result->length = 0;
        result->distance = TS_INF;
    }

    heap_free(heap);
    free(dist);
    free(prev);
    free(visited);
    return result;
}


void graph_print(const graph_t *graph, const int *node_ids_translation) {
    for (size_t i = 0; i < graph->num_nodes; ++i) {
        const adj_list_t *list = &graph->adj[i];
        printf("Node %d", node_ids_translation != NULL ? node_ids_translation[i] : (int)i);
        printf(" [w=%d]: ", graph->node_weights[i]);
        for (size_t j = 0; j < list->degree; ++j) {
            printf(" -> %d (%d)", node_ids_translation != NULL ? node_ids_translation[list->edges[j].to] : list->edges[j].to, list->edges[j].weight);
        }
        printf("\n");
    }
}


graph_t *graph_new(size_t num_nodes) {
    graph_t *g = malloc(sizeof(graph_t));
    g->num_nodes = num_nodes;
    g->adj = malloc(num_nodes * sizeof(adj_list_t));
    g->node_weights = calloc(num_nodes, sizeof(int));
    for (size_t i = 0; i < num_nodes; ++i) {
        g->adj[i].edges = malloc(4 * sizeof(edge_t));
        g->adj[i].degree = 0;
        g->adj[i].capacity = 4;
        g->node_weights[i] = 0;
    }
    return g;
}


graph_t *graph_copy(const graph_t *src) {
    graph_t *copy = malloc(sizeof(graph_t));
    copy->num_nodes = src->num_nodes;
    copy->adj = malloc(copy->num_nodes * sizeof(adj_list_t));
    copy->node_weights = malloc(sizeof(int) * copy->num_nodes);
    memcpy(copy->node_weights, src->node_weights, sizeof(int) * copy->num_nodes);
    for (size_t i = 0; i < src->num_nodes; ++i) {
        const adj_list_t *src_list = &src->adj[i];
        adj_list_t *dst_list = &copy->adj[i];
        dst_list->degree = src_list->degree;
        dst_list->capacity = src_list->degree > 0 ? src_list->degree : 4;
        dst_list->edges = malloc(sizeof(edge_t) * dst_list->capacity);
        if (src_list->degree > 0) {
            memcpy(dst_list->edges, src_list->edges, sizeof(edge_t) * src_list->degree);
        }
    }
    return copy;
}


void graph_free(graph_t *graph) {
    if (!graph) return;
    for (size_t i = 0; i < graph->num_nodes; ++i) {
        free(graph->adj[i].edges);
    }
    free(graph->adj);
    free(graph->node_weights);
    free(graph);
}


path_t *path_copy(const path_t *src) {
    if (!src) return NULL;

    path_t *copy = malloc(sizeof(path_t));
    
    copy->length = src->length;
    copy->distance = src->distance;

    if (src->length == 0) {
        copy->nodes = NULL;
        copy->distances = NULL;
        return copy;
    }

    copy->nodes = malloc(sizeof(int) * src->length);
    memcpy(copy->nodes, src->nodes, sizeof(int) * src->length);

    if (src->length <= 1) {
        copy->distances = NULL;
        return copy;
    }

    copy->distances = malloc(sizeof(int) * (src->length - 1));
    memcpy(copy->distances, src->distances, sizeof(int) * (src->length - 1));

    return copy;
}


void path_free(path_t *path) {
    if (!path) return;
    free(path->nodes);
    free(path);
}