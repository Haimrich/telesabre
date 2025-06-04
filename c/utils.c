#include "utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void fisher_yates(void *arr, size_t n, size_t elem_size) {
    char *a = (char *)arr;
    void *tmp = malloc(elem_size);
    if (!tmp) return;

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        memcpy(tmp, a + i * elem_size, elem_size);
        memcpy(a + i * elem_size, a + j * elem_size, elem_size);
        memcpy(a + j * elem_size, tmp, elem_size);
    }
    free(tmp);
}


int **floyd_warshall(int num_nodes, int edges[][3], int num_edges) {
    int **dist = (int **)malloc(num_nodes * sizeof(int *));
    for (int i = 0; i < num_nodes; ++i) {
        dist[i] = (int *)malloc(num_nodes * sizeof(int));
        for (int j = 0; j < num_nodes; ++j) dist[i][j] = (i == j) ? 0 : TS_INF;
    }

    for (int e = 0; e < num_edges; ++e) {
        int u = edges[e][0];
        int v = edges[e][1];
        int w = edges[e][2];
        dist[u][v] = w;
        dist[v][u] = w;
    }

    // Floyd-Warshall
    for (int k = 0; k < num_nodes; ++k)
        for (int i = 0; i < num_nodes; ++i)
            for (int j = 0; j < num_nodes; ++j)
                if (dist[i][j] > dist[i][k] + dist[k][j]) dist[i][j] = dist[i][k] + dist[k][j];

    return dist;
}


void error_impl(const char *file, int line, const char *msg, ...) {
    fprintf(stderr, "[%s:%d] ", file, line);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}


void check_alloc_impl(const char *file, int line, int num_ptrs, ...) {
    va_list args;
    va_start(args, num_ptrs);
    void **ptrs = malloc(num_ptrs * sizeof(void *));
    int null_found = 0;
    for (int i = 0; i < num_ptrs; ++i) {
        ptrs[i] = va_arg(args, void *);
        if (ptrs[i] == NULL) null_found = 1;
    }
    va_end(args);

    if (null_found) {
        for (int i = 0; i < num_ptrs; ++i) {
            if (ptrs[i]) free(ptrs[i]);
        }
        free(ptrs);
        error_impl(file, line, "Failed to allocate memory.");
    }
    free(ptrs);
}

size_t realloc_grow(void *ptr, size_t unit, size_t old_size, size_t new_size) {
    if (new_size > old_size) {
        void *new_ptr = realloc(ptr, new_size * unit);
        check_alloc(1, new_ptr);
        return new_size;
    }
    return old_size;
}


const char *byte_to_binary(unsigned char x) {
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}