#include "heap.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"


static void heap_swap(heap_t* heap, size_t a, size_t b) {
    heap_item_t tmp = heap->data[a];
    heap->data[a] = heap->data[b];
    heap->data[b] = tmp;
    heap->pos[heap->data[a].id] = a;
    heap->pos[heap->data[b].id] = b;
}

static void heapify_up(heap_t *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (heap->data[parent].priority <= heap->data[idx].priority)
            break;
        heap_swap(heap, idx, parent);
        idx = parent;
    }
}

static void heapify_down(heap_t *heap, size_t idx) {
    size_t n = heap->size;
    while (1) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t min = idx;

        if (left < n && heap->data[left].priority < heap->data[min].priority)
            min = left;
        if (right < n && heap->data[right].priority < heap->data[min].priority)
            min = right;
        if (min == idx)
            break;
       
        heap_swap(heap, idx, min);
        idx = min;
    }
}

static void heap_ensure_capacity(heap_t* heap, int id) {
    if ((size_t)id < heap->capacity && heap->size < heap->capacity) return;

    size_t new_cap = heap->capacity;
    while ((size_t)id >= new_cap || heap->size >= new_cap)
        new_cap = new_cap ? new_cap * 2 : 4;

    heap->data = realloc(heap->data, sizeof(heap_item_t) * new_cap);
    heap->pos = realloc(heap->pos, sizeof(int) * new_cap);
    check_alloc(3, heap, heap->data, heap->pos);

    for (size_t i = heap->capacity; i < new_cap; ++i)
        heap->pos[i] = -1;

    heap->capacity = new_cap;
}


heap_t *heap_new(size_t capacity) {
    heap_t *heap = malloc(sizeof(heap_t));
    check_alloc(1, heap);

    heap->data = malloc(sizeof(heap_item_t) * capacity);
    heap->pos = malloc(sizeof(int) * capacity);
    check_alloc(2, heap->data, heap->pos);

    heap->capacity = capacity > 4 ? capacity : 4;
    heap->size = 0;
    for (size_t i = 0; i < capacity; ++i)
        heap->pos[i] = -1;
    return heap;
}

void heap_free(heap_t *heap) {
    if (!heap) return;
    free(heap->data);
    free(heap->pos);
    free(heap);
}

heap_t *heap_copy(const heap_t *src) {
    if (!src) return NULL;

    heap_t *copy = malloc(sizeof(heap_t));
    check_alloc(1, copy);

    copy->data = malloc(sizeof(heap_item_t) * src->capacity);
    copy->pos = malloc(sizeof(int) * src->capacity);
    check_alloc(3, copy, copy->data, copy->pos);

    copy->capacity = src->capacity;
    copy->size = src->size;
   
    memcpy(copy->data, src->data, sizeof(heap_item_t) * src->size);
    memcpy(copy->pos, src->pos, sizeof(int) * src->capacity);

    return copy;
}

void heap_insert(heap_t *heap, int id, int priority) {
    if (id < 0) error("Tried to insert item with negative id %d in a heap.", id);

    heap_ensure_capacity(heap, id);

    int idx = heap->pos[id];
    if (idx != -1) {
        if (priority < heap->data[idx].priority) {
            heap->data[idx].priority = priority;
            heapify_up(heap, idx);
        } else if (priority > heap->data[idx].priority) {
            heap->data[idx].priority = priority;
            heapify_down(heap, idx);
        }
        return;
    }
    
    idx = heap->size++;
    heap->data[idx].id = id;
    heap->data[idx].priority = priority;
    heap->pos[id] = idx;
    heapify_up(heap, idx);
}

void heap_remove(heap_t *heap, int id) {
    if (id < 0 || id >= (int)heap->capacity) 
        error("Trying to remove item with id %d from heap with capacity %zu.", id, heap->capacity);

    int idx = heap->pos[id];
    if (idx == -1) return;

    int last = heap->size - 1;
    if (idx != last) {
        heap_swap(heap, idx, last);
    }

    heap->pos[heap->data[last].id] = -1;
    heap->size--;
    if ((size_t)idx < heap->size) {
        heapify_down(heap, idx);
        heapify_up(heap, idx);
    }
}

heap_item_t heap_get_min(const heap_t *heap) {
    if (heap->size == 0) {
        heap_item_t none = { .id = -1, .priority = TS_INF };
        return none;
    }
    return heap->data[0];
}

heap_item_t heap_extract_min(heap_t *heap) {
    heap_item_t min = heap_get_min(heap);
    if (min.id != -1)
        heap_remove(heap, min.id);
    return min;
}

int heap_is_empty(const heap_t *heap) {
    return heap->size == 0;
}