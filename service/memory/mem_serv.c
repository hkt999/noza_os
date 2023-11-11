// TODO: use inhouse malloc/free instead of the system one
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

    if (new_heap_end > (char *)heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end = new_heap_end;
    return (void *)prev_heap_end;
}

Argument	Description
base	Address of tinyalloc control structure, typically at the beginning of your heap
limit	Heap space end address
heap_blocks	Max. number of memory chunks (e.g. 256)
split_thresh	Size threshold for splitting chunks (a good default is 16)
alignment	Word size for pointer alignment (e.g. 8)
alignment is assumed to be >= native word size
base must be an address in RAM (on embedded devices)


#endif

#include "mem_serv.h"
#include "nozaos.h"
#include "3rd_party/tinyalloc/tinyalloc.h"

extern char __end__, __StackLimit; // __StackLimit is poorly named, it's actually the end of the heap
void *heap_end = &__end__;
void *heap_limit = &__StackLimit;

void *_sbrk(ptrdiff_t increment)
{
    char *prev_heap_end = heap_end;
    char *new_heap_end = heap_end + increment;

    if (new_heap_end > (char *)heap_limit) {
        //errno = ENOMEM;
        return (void *)-1;
    }

    heap_end = new_heap_end;
    return (void *)prev_heap_end;
}

uint32_t memory_pid = 0;
static int do_memory_server(void *param, uint32_t pid)
{
    int ret;
	memory_pid = pid;
    noza_msg_t msg;
    
    void *heap = _sbrk(100*1024);
    ta_init(heap, heap + 100*1024, 256, 16, 8);
    for (;;) {
        if ((ret = noza_recv(&msg)) == 0) { // the pid in msg is the sender pid
			mem_msg_t *mem_msg = (mem_msg_t *)msg.ptr;
			// process the request
			switch (mem_msg->cmd) {
				case MEMORY_MALLOC:
                    //mem_msg->ptr = malloc(mem_msg->size);
                    mem_msg->ptr = ta_alloc(mem_msg->size);
                    if (mem_msg->ptr == NULL) {
                        mem_msg->code = MEMORY_INVALID_OP;
                    } else {
                        mem_msg->code = MEMORY_SUCCESS;
                    }
					break;

				case MEMORY_FREE:
                    //free(mem_msg->ptr);
                    ta_free(mem_msg->ptr);
                    mem_msg->ptr = NULL;
                    mem_msg->code = MEMORY_SUCCESS;
                    break;

				default:
                    mem_msg->ptr = NULL;
                    mem_msg->size = 0;
					mem_msg->code = MEMORY_INVALID_OP;
					break;
			}
		    noza_reply(&msg); // reply
        }
    }
    return 0;
}

static uint8_t memory_server_stack[1024];
void __attribute__((constructor(110))) memory_server_init(void *param, uint32_t pid)
{
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_memory_server, memory_server_stack, sizeof(memory_server_stack));
}