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
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mem_serv.h"
#include "nozaos.h"

uint32_t memory_pid = 0;
static int do_memory_server(void *param, uint32_t pid)
{
    int ret;
	memory_pid = pid;
    noza_msg_t msg;

    for (;;) {
        if ((ret = noza_recv(&msg)) == 0) { // the pid in msg is the sender pid
			mem_msg_t *mem_msg = (mem_msg_t *)msg.ptr;
			// process the request
			switch (mem_msg->cmd) {
				case MEMORY_MALLOC:
                    mem_msg->ptr = malloc(mem_msg->size);
                    if (mem_msg->ptr == NULL) {
                        mem_msg->code = MEMORY_INVALID_OP;
                    } else {
                        mem_msg->code = MEMORY_SUCCESS;
                    }
					break;

				case MEMORY_FREE:
                    free(mem_msg->ptr);
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
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_memory_server, memory_server_stack, sizeof(memory_server_stack));
}