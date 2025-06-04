#pragma once

#include <stddef.h>


typedef struct {
    int id;
    int priority;
} heap_item_t;

typedef struct {
    heap_item_t* data;
    int* pos; // Maps item id to its index in data array
    size_t capacity;
    size_t size;
} heap_t;

heap_t* heap_new(size_t capacity);

void heap_free(heap_t* heap);

heap_t* heap_copy(const heap_t* src);

void heap_insert(heap_t* heap, int id, int priority);

void heap_remove(heap_t* heap, int id);

heap_item_t heap_get_min(const heap_t* heap);

heap_item_t heap_extract_min(heap_t* heap);

int heap_is_empty(const heap_t* heap);