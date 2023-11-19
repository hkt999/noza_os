#include "tinyalloc.h"
#include <stdint.h>

#ifdef TA_DEBUG
extern void print_s(char *);
extern void print_i(size_t);
#else
#define print_s(X)
#define print_i(X)
#endif

/**
 * If compaction is enabled, inserts block
 * into free list, sorted by addr.
 * If disabled, add block has new head of
 * the free list.
 */
static void insert_block(tinyalloc_t *tinyalloc, Block *block) {
#ifndef TA_DISABLE_COMPACT
    Block *ptr  = tinyalloc->heap->free;
    Block *prev = NULL;
    while (ptr != NULL) {
        if ((size_t)block->addr <= (size_t)ptr->addr) {
            print_s("insert");
            print_i((size_t)ptr);
            break;
        }
        prev = ptr;
        ptr  = ptr->next;
    }
    if (prev != NULL) {
        if (ptr == NULL) {
            print_s("new tail");
        }
        prev->next = block;
    } else {
        print_s("new head");
        tinyalloc->heap->free = block;
    }
    block->next = ptr;
#else
    block->next = heap->free;
    heap->free  = block;
#endif
}

#ifndef TA_DISABLE_COMPACT
static void release_blocks(tinyalloc_t *tinyalloc, Block *scan, Block *to) {
    Block *scan_next;
    while (scan != to) {
        print_s("release");
        print_i((size_t)scan);
        scan_next   = scan->next;
        scan->next  = tinyalloc->heap->fresh;
        tinyalloc->heap->fresh = scan;
        scan->addr  = 0;
        scan->size  = 0;
        scan        = scan_next;
    }
}

static void compact(tinyalloc_t *tinyalloc) {
    Block *ptr = tinyalloc->heap->free;
    Block *prev;
    Block *scan;
    while (ptr != NULL) {
        prev = ptr;
        scan = ptr->next;
        while (scan != NULL &&
               (size_t)prev->addr + prev->size == (size_t)scan->addr) {
            print_s("merge");
            print_i((size_t)scan);
            prev = scan;
            scan = scan->next;
        }
        if (prev != ptr) {
            size_t new_size =
                (size_t)prev->addr - (size_t)ptr->addr + prev->size;
            print_s("new size");
            print_i(new_size);
            ptr->size   = new_size;
            Block *next = prev->next;
            // make merged blocks available
            release_blocks(tinyalloc, ptr->next, prev->next);
            // relink
            ptr->next = next;
        }
        ptr = ptr->next;
    }
}
#endif

bool ta_init(tinyalloc_t *tinyalloc, const void *base, const void *limit,const size_t heap_blocks, const size_t split_thresh, const size_t alignment) {
    tinyalloc->heap = (Heap *)base;
    tinyalloc->heap_limit = limit;
    tinyalloc->heap_split_thresh = split_thresh;
    tinyalloc->heap_alignment = alignment;
    tinyalloc->heap_max_blocks = heap_blocks;

    tinyalloc->heap->free   = NULL;
    tinyalloc->heap->used   = NULL;
    tinyalloc->heap->fresh  = (Block *)(tinyalloc->heap + 1);
    tinyalloc->heap->top    = (size_t)(tinyalloc->heap->fresh + heap_blocks);

    Block *block = tinyalloc->heap->fresh;
    size_t i     = tinyalloc->heap_max_blocks - 1;
    while (i--) {
        block->next = block + 1;
        block++;
    }
    block->next = NULL;
    return true;
}

bool ta_free(tinyalloc_t *tinyalloc, void *free) {
    Block *block = tinyalloc->heap->used;
    Block *prev  = NULL;
    while (block != NULL) {
        if (free == block->addr) {
            if (prev) {
                prev->next = block->next;
            } else {
                tinyalloc->heap->used = block->next;
            }
            insert_block(tinyalloc, block);
#ifndef TA_DISABLE_COMPACT
            compact(tinyalloc);
#endif
            return true;
        }
        prev  = block;
        block = block->next;
    }
    return false;
}

static Block *alloc_block(tinyalloc_t *tinyalloc, size_t num) {
    Block *ptr  = tinyalloc->heap->free;
    Block *prev = NULL;
    size_t top  = tinyalloc->heap->top;
    num         = (num + tinyalloc->heap_alignment - 1) & -tinyalloc->heap_alignment;
    while (ptr != NULL) {
        const int is_top = ((size_t)ptr->addr + ptr->size >= top) && ((size_t)ptr->addr + num <= (size_t)tinyalloc->heap_limit);
        if (is_top || ptr->size >= num) {
            if (prev != NULL) {
                prev->next = ptr->next;
            } else {
                tinyalloc->heap->free = ptr->next;
            }
            ptr->next  = tinyalloc->heap->used;
            tinyalloc->heap->used = ptr;
            if (is_top) {
                print_s("resize top block");
                ptr->size = num;
                tinyalloc->heap->top = (size_t)ptr->addr + num;
#ifndef TA_DISABLE_SPLIT
            } else if (tinyalloc->heap->fresh != NULL) {
                size_t excess = ptr->size - num;
                if (excess >= tinyalloc->heap_split_thresh) {
                    ptr->size    = num;
                    Block *split = tinyalloc->heap->fresh;
                    tinyalloc->heap->fresh  = split->next;
                    split->addr  = (void *)((size_t)ptr->addr + num);
                    print_s("split");
                    print_i((size_t)split->addr);
                    split->size = excess;
                    insert_block(tinyalloc, split);
#ifndef TA_DISABLE_COMPACT
                    compact(tinyalloc);
#endif
                }
#endif
            }
            return ptr;
        }
        prev = ptr;
        ptr  = ptr->next;
    }
    // no matching free blocks
    // see if any other blocks available
    size_t new_top = top + num;
    if (tinyalloc->heap->fresh != NULL && new_top <= (size_t)tinyalloc->heap_limit) {
        ptr         = tinyalloc->heap->fresh;
        tinyalloc->heap->fresh = ptr->next;
        ptr->addr   = (void *)top;
        ptr->next   = tinyalloc->heap->used;
        ptr->size   = num;
        tinyalloc->heap->used  = ptr;
        tinyalloc->heap->top   = new_top;
        return ptr;
    }
    return NULL;
}

void *ta_alloc(tinyalloc_t *tinyalloc, size_t num) {
    Block *block = alloc_block(tinyalloc, num);
    if (block != NULL) {
        return block->addr;
    }
    return NULL;
}

static void memclear(void *ptr, size_t num) {
    size_t *ptrw = (size_t *)ptr;
    size_t numw  = (num & -sizeof(size_t)) / sizeof(size_t);
    while (numw--) {
        *ptrw++ = 0;
    }
    num &= (sizeof(size_t) - 1);
    uint8_t *ptrb = (uint8_t *)ptrw;
    while (num--) {
        *ptrb++ = 0;
    }
}

void *ta_calloc(tinyalloc_t *tinyalloc, size_t num, size_t size) {
    num *= size;
    Block *block = alloc_block(tinyalloc, num);
    if (block != NULL) {
        memclear(block->addr, num);
        return block->addr;
    }
    return NULL;
}

static size_t count_blocks(Block *ptr) {
    size_t num = 0;
    while (ptr != NULL) {
        num++;
        ptr = ptr->next;
    }
    return num;
}

size_t ta_num_free(tinyalloc_t *tinyalloc) {
    return count_blocks(tinyalloc->heap->free);
}

size_t ta_num_used(tinyalloc_t *tinyalloc) {
    return count_blocks(tinyalloc->heap->used);
}

size_t ta_num_fresh(tinyalloc_t *tinyalloc) {
    return count_blocks(tinyalloc->heap->fresh);
}

bool ta_check(tinyalloc_t *tinyalloc) {
    return tinyalloc->heap_max_blocks == ta_num_free(tinyalloc) + ta_num_used(tinyalloc) + ta_num_fresh(tinyalloc);
}
