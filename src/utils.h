
#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdio.h>


#define TS_INF (INT_MAX / 2)

void error_impl(const char *file, int line, const char *msg, ...);
#define error(msg, ...) error_impl(__FILE__, __LINE__, msg, ##__VA_ARGS__)

void check_alloc_impl(const char *file, int line, int num_ptrs, ...);
#define check_alloc(...) check_alloc_impl(__FILE__, __LINE__, __VA_ARGS__)

void check_realloc(void *ptr, size_t unit, size_t old_size, size_t new_size);

void fisher_yates(void *arr, size_t n, size_t elem_size);

int **floyd_warshall(int num_nodes, int edges[][3], int num_edges);

const char *byte_to_binary(unsigned char x);

const char *read_file(const char *filename);

void filepath_basename(const char *path, char *out, size_t out_size);

void multipartite_graph_layout(
    int num_nodes,
    const size_t *node_layers,
    int num_layers,
    const size_t *layer_sizes,
    float x_gap,
    float y_gap,
    float positions_out[][2]
);


//Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

//Regular bold text
#define BBLK "\e[1;30m"
#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define BYEL "\e[1;33m"
#define BBLU "\e[1;34m"
#define BMAG "\e[1;35m"
#define BCYN "\e[1;36m"
#define BWHT "\e[1;37m"

//Regular underline text
#define UBLK "\e[4;30m"
#define URED "\e[4;31m"
#define UGRN "\e[4;32m"
#define UYEL "\e[4;33m"
#define UBLU "\e[4;34m"
#define UMAG "\e[4;35m"
#define UCYN "\e[4;36m"
#define UWHT "\e[4;37m"

//Regular background
#define BLKB "\e[40m"
#define REDB "\e[41m"
#define GRNB "\e[42m"
#define YELB "\e[43m"
#define BLUB "\e[44m"
#define MAGB "\e[45m"
#define CYNB "\e[46m"
#define WHTB "\e[47m"

//High intensty background 
#define BLKHB "\e[0;100m"
#define REDHB "\e[0;101m"
#define GRNHB "\e[0;102m"
#define YELHB "\e[0;103m"
#define BLUHB "\e[0;104m"
#define MAGHB "\e[0;105m"
#define CYNHB "\e[0;106m"
#define WHTHB "\e[0;107m"

//High intensty text
#define HBLK "\e[0;90m"
#define HRED "\e[0;91m"
#define HGRN "\e[0;92m"
#define HYEL "\e[0;93m"
#define HBLU "\e[0;94m"
#define HMAG "\e[0;95m"
#define HCYN "\e[0;96m"
#define HWHT "\e[0;97m"

//Bold high intensity text
#define BHBLK "\e[1;90m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"

//Reset
#define CRESET "\e[0m"
#define COLOR_RESET "\e[0m"

#define H1COL BHWHT
#define H2COL HGRN
#define H3COL HYEL