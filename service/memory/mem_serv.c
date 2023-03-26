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

    if (new_heap_end > (char *)heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end = new_heap_end;
    return (void *)prev_heap_end;
}
