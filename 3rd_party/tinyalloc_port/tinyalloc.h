#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct Block Block;

struct Block {
    void *addr;
    Block *next;
    size_t size;
};

typedef struct {
    Block *free;   // first free block
    Block *used;   // first used block
    Block *fresh;  // first available blank block
    size_t top;    // top free addr
} Heap;

typedef struct {
    Heap *heap;
    const void *heap_limit;
    size_t heap_split_thresh;
    size_t heap_alignment;
    size_t heap_max_blocks;
} tinyalloc_t;

bool ta_init(tinyalloc_t *tinyalloc, const void *base, const void *limit, const size_t heap_blocks, const size_t split_thresh, const size_t alignment);
void *ta_alloc(tinyalloc_t *tinyalloc, size_t num);
void *ta_calloc(tinyalloc_t *tinyalloc, size_t num, size_t size);
bool ta_free(tinyalloc_t *tinyalloc, void *ptr);

size_t ta_num_free(tinyalloc_t *tinyalloc);
size_t ta_num_used(tinyalloc_t *tinyalloc);
size_t ta_num_fresh(tinyalloc_t *tinyalloc);
bool ta_check(tinyalloc_t *tinyalloc);

#ifdef __cplusplus
}
#endif
