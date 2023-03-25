#if 0

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern char __end__, __StackLimit; // __StackLimit is poorly named, it's actually the end of the heap
void *heap_end = &__end__;
void *heap_limit = &__StackLimit;

void *_sbrk(ptrdiff_t increment)
{
    char *prev_heap_end = heap_end;
    char *new_heap_end = heap_end + increment;

    printf("------------------ %p, %p, %d\n", heap_end, heap_limit, (int)((char *)heap_limit - (char *)heap_end));

    if (new_heap_end > (char *)heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end = new_heap_end;
    return (void *)prev_heap_end;
}

void *_malloc_r(struct _reent *reent_ptr, size_t size)
{
    void *p = _sbrk(size);
    printf("************---- malloc ************---- %p\n", p);
    printf("SSSS\n");
    return p;
}

void *_calloc_r(struct _reent *reent_ptr, size_t num, size_t size)
{
    printf("************---- calloc ************----\n");
    void *p = _sbrk(num * size);
    memset(p, 0, num * size);
    return p;
}

void *_realloc_r(struct _reent *reent_ptr, void *ptr, size_t size)
{
    printf("************---- realloc ************----\n");
    void *p = _sbrk(size);
    memcpy(p, ptr, size);
    return p;
}

void _free_r(struct _reent *reent_ptr, void *ptr)
{
    printf("************---- free ************---- %p, %p\n", reent_ptr, ptr);
}
#endif