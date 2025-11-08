#include "tlsf.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define TLSF_ASSERT(expr) assert(expr)

#define ALIGN_SIZE        ((size_t)8)
#define ALIGN_MASK        (ALIGN_SIZE - 1)
#define ALIGN_UP(x)       (((x) + ALIGN_MASK) & ~(ALIGN_MASK))

#define FL_COUNT          16
#define SL_COUNT          8
#define SMALL_BLOCK_SIZE  (ALIGN_SIZE * SL_COUNT)

#define BLOCK_FREE_BIT    ((size_t)1)
#define BLOCK_PREV_FREE   ((size_t)2)
#define BLOCK_SIZE_MASK   (~(size_t)0x3)

typedef struct block_header block_header_t;

struct block_header {
    block_header_t *prev_phys;
    size_t size;
    block_header_t *next_free;
    block_header_t *prev_free;
};

typedef struct {
    block_header_t *free_list[FL_COUNT][SL_COUNT];
    uint32_t fl_bitmap;
    uint32_t sl_bitmap[FL_COUNT];
    void *pool_start;
    void *pool_end;
} tlsf_control_t;

typedef struct {
    tlsf_control_t control;
} tlsf_superblock_t;

static inline size_t block_size(const block_header_t *block)
{
    return block->size & BLOCK_SIZE_MASK;
}

static inline void block_set_size(block_header_t *block, size_t size)
{
    block->size = (block->size & ~BLOCK_SIZE_MASK) | size;
}

static inline int block_is_free(const block_header_t *block)
{
    return (block->size & BLOCK_FREE_BIT) != 0;
}

static inline void block_mark_free(block_header_t *block)
{
    block->size |= BLOCK_FREE_BIT;
}

static inline void block_mark_used(block_header_t *block)
{
    block->size &= ~BLOCK_FREE_BIT;
}

static inline void block_set_prev_free(block_header_t *block)
{
    block->size |= BLOCK_PREV_FREE;
}

static inline void block_set_prev_used(block_header_t *block)
{
    block->size &= ~BLOCK_PREV_FREE;
}

static inline block_header_t *block_from_ptr(const void *ptr)
{
    return (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
}

static inline void *block_to_ptr(const block_header_t *block)
{
    return (void *)((uint8_t *)block + sizeof(block_header_t));
}

static inline block_header_t *block_next(block_header_t *block)
{
    return (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + block_size(block));
}

static inline block_header_t *block_prev(const block_header_t *block)
{
    if (block->size & BLOCK_PREV_FREE)
        return block->prev_phys;
    return NULL;
}

static inline int ctz32(uint32_t value)
{
    TLSF_ASSERT(value);
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(value);
#else
    int bit = 0;
    while (((value >> bit) & 1u) == 0u)
        bit++;
    return bit;
#endif
}

static void mapping_insert(size_t size, int *fl, int *sl)
{
    if (size < SMALL_BLOCK_SIZE) {
        *fl = 0;
        size_t step = SMALL_BLOCK_SIZE / SL_COUNT;
        *sl = (int)(size / step);
        if (*sl >= SL_COUNT)
            *sl = SL_COUNT - 1;
        return;
    }

    size_t base = SMALL_BLOCK_SIZE;
    int fl_index = 0;
    while (size >= (base << 1) && fl_index < FL_COUNT - 1) {
        base <<= 1;
        fl_index++;
    }

    size_t step = base / SL_COUNT;
    size_t offset = size - base;
    int sl_index = (int)(offset / step);
    if (sl_index >= SL_COUNT)
        sl_index = SL_COUNT - 1;

    *fl = fl_index;
    *sl = sl_index;
}

static block_header_t *search_suitable(tlsf_control_t *control, int *fl, int *sl)
{
    uint32_t fl_map = control->fl_bitmap & (~0u << *fl);
    if (!fl_map)
        return NULL;

    int fl_index = ctz32(fl_map);
    uint32_t sl_map = control->sl_bitmap[fl_index];
    if (fl_index == *fl) {
        sl_map &= (~0u << *sl);
        if (!sl_map) {
            fl_map &= ~(1u << fl_index);
            if (!fl_map)
                return NULL;
            fl_index = ctz32(fl_map);
            sl_map = control->sl_bitmap[fl_index];
        }
    }
    int sl_index = ctz32(sl_map);
    *fl = fl_index;
    *sl = sl_index;
    return control->free_list[fl_index][sl_index];
}

static void insert_free_block(tlsf_control_t *control, block_header_t *block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    block_header_t *head = control->free_list[fl][sl];
    block->next_free = head;
    block->prev_free = NULL;
    if (head)
        head->prev_free = block;
    control->free_list[fl][sl] = block;
    control->fl_bitmap |= (1u << fl);
    control->sl_bitmap[fl] |= (1u << sl);
    block_mark_free(block);
    block_set_prev_free(block_next(block));
}

static void remove_free_block(tlsf_control_t *control, block_header_t *block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    block_header_t *next = block->next_free;
    block_header_t *prev = block->prev_free;
    if (next)
        next->prev_free = prev;
    if (prev)
        prev->next_free = next;
    else
        control->free_list[fl][sl] = next;
    if (!control->free_list[fl][sl]) {
        control->sl_bitmap[fl] &= ~(1u << sl);
        if (!control->sl_bitmap[fl])
            control->fl_bitmap &= ~(1u << fl);
    }
    block->next_free = block->prev_free = NULL;
}

static block_header_t *split_block(block_header_t *block, size_t size)
{
    size_t remainder = block_size(block) - size;
    if (remainder <= sizeof(block_header_t))
        return NULL;

    block_set_size(block, size);
    block_header_t *next =
        (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + size);
    next->prev_phys = block;
    block_set_size(next, remainder - sizeof(block_header_t));
    block_mark_free(next);
    next->next_free = NULL;
    next->prev_free = NULL;
    block_set_prev_free(next);
    block_header_t *next_next = block_next(next);
    next_next->prev_phys = next;
    block_set_prev_free(next_next);
    return next;
}

static block_header_t *merge_with_next(block_header_t *block)
{
    block_header_t *next = block_next(block);
    if (block_is_free(next)) {
        block_set_size(block, block_size(block) + block_size(next) + sizeof(block_header_t));
        block_header_t *after = block_next(block);
        after->prev_phys = block;
        block_set_prev_free(after);
    }
    return block;
}

static void *tlsf_malloc_internal(tlsf_control_t *control, size_t bytes)
{
    if (bytes == 0)
        return NULL;
    bytes = ALIGN_UP(bytes);
    if (bytes < ALIGN_SIZE)
        bytes = ALIGN_SIZE;
    int fl = 0, sl = 0;
    mapping_insert(bytes, &fl, &sl);
    block_header_t *block = search_suitable(control, &fl, &sl);
    if (!block)
        return NULL;
    remove_free_block(control, block);
    block_header_t *remaining = split_block(block, bytes);
    if (remaining)
        insert_free_block(control, remaining);
    block_mark_used(block);
    block_set_prev_used(block_next(block));
    return block_to_ptr(block);
}

static void tlsf_free_internal(tlsf_control_t *control, void *ptr)
{
    if (!ptr)
        return;
    block_header_t *block = block_from_ptr(ptr);
    block_mark_free(block);
    block_header_t *prev = block_prev(block);
    if (prev && block_is_free(prev)) {
        remove_free_block(control, prev);
        block = merge_with_next(prev);
    }
    block_header_t *next = block_next(block);
    if (block_is_free(next)) {
        remove_free_block(control, next);
        block = merge_with_next(block);
    }
    insert_free_block(control, block);
}

tlsf_t tlsf_create_with_pool(void *mem, size_t bytes)
{
    if (!mem || bytes < sizeof(tlsf_superblock_t) + sizeof(block_header_t) * 2)
        return NULL;

    uintptr_t base_addr = (uintptr_t)mem;
    uintptr_t aligned = ALIGN_UP(base_addr);
    bytes -= (aligned - base_addr);
    uint8_t *base = (uint8_t *)aligned;

    size_t control_bytes = ALIGN_UP(sizeof(tlsf_superblock_t));
    if (bytes <= control_bytes + sizeof(block_header_t) * 2)
        return NULL;

    tlsf_superblock_t *super = (tlsf_superblock_t *)base;
    tlsf_control_t *control = &super->control;
    memset(control, 0, sizeof(*control));

    uint8_t *pool_start = base + control_bytes;
    uint8_t *pool_end = base + bytes;
    control->pool_start = pool_start;
    control->pool_end = pool_end;

    size_t usable = (size_t)(pool_end - pool_start);
    if (usable <= sizeof(block_header_t) * 2)
        return NULL;

    block_header_t *block = (block_header_t *)pool_start;
    memset(block, 0, sizeof(*block));
    block->prev_phys = NULL;
    block_set_size(block, usable - sizeof(block_header_t) * 2);
    block_mark_free(block);

    block_header_t *sentinel =
        (block_header_t *)(pool_end - sizeof(block_header_t));
    memset(sentinel, 0, sizeof(*sentinel));
    sentinel->prev_phys = block;
    block_set_size(sentinel, 0);
    block_mark_used(sentinel);
    block_set_prev_used(sentinel);

    insert_free_block(control, block);
    return (tlsf_t)super;
}

void tlsf_destroy(tlsf_t tlsf)
{
    (void)tlsf;
}

void *tlsf_malloc(tlsf_t tlsf, size_t bytes)
{
    if (!tlsf)
        return NULL;
    tlsf_superblock_t *super = (tlsf_superblock_t *)tlsf;
    return tlsf_malloc_internal(&super->control, bytes);
}

void tlsf_free(tlsf_t tlsf, void *ptr)
{
    if (!tlsf)
        return;
    tlsf_superblock_t *super = (tlsf_superblock_t *)tlsf;
    tlsf_free_internal(&super->control, ptr);
}

void *tlsf_realloc(tlsf_t tlsf, void *ptr, size_t bytes)
{
    if (!ptr)
        return tlsf_malloc(tlsf, bytes);
    if (bytes == 0) {
        tlsf_free(tlsf, ptr);
        return NULL;
    }
    block_header_t *block = block_from_ptr(ptr);
    size_t current = block_size(block);
    if (bytes <= current)
        return ptr;
    void *new_ptr = tlsf_malloc(tlsf, bytes);
    if (new_ptr) {
        memcpy(new_ptr, ptr, current);
        tlsf_free(tlsf, ptr);
    }
    return new_ptr;
}

void *tlsf_memalign(tlsf_t tlsf, size_t alignment, size_t bytes)
{
    if (alignment <= ALIGN_SIZE)
        return tlsf_malloc(tlsf, bytes);
    return NULL;
}

size_t tlsf_used_size(tlsf_t tlsf)
{
    (void)tlsf;
    return 0;
}

size_t tlsf_free_size(tlsf_t tlsf)
{
    (void)tlsf;
    return 0;
}
