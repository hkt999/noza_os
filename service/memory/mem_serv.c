#include <stdint.h>
#include "mem_serv.h"
#include "nozaos.h"
#include "posix/errno.h"
#include "3rd_party/tinyalloc_port/tinyalloc.h"
#include "../name_lookup/name_lookup_client.h"
#include "printk.h"

extern char __end__, __StackLimit; // __StackLimit is poorly named, it's actually the end of the heap
void *heap_end = &__end__;
void *heap_limit = &__StackLimit;

void *_sbrk(ptrdiff_t increment)
{
    char *prev_heap_end = heap_end;
    char *new_heap_end = heap_end + increment;

    if (new_heap_end > (char *)heap_limit) {
        return (void *)-1;
    }

    heap_end = new_heap_end;
    return (void *)prev_heap_end;
}

static tinyalloc_t tinyalloc;

static void *memory_service_heap_limit(void)
{
    uintptr_t base = (uintptr_t)heap_end;
    uintptr_t limit = (uintptr_t)heap_limit;
    size_t heap_bytes = (size_t)(limit - base);

    // The memory service uses tinyalloc directly, but other services still rely
    // on newlib malloc/calloc/realloc via _sbrk(). Leave a tail region for that
    // allocator instead of consuming the entire heap here.
    size_t libc_reserve = 64u * 1024u;
    if (heap_bytes <= libc_reserve * 2u) {
        libc_reserve = heap_bytes / 4u;
    }

    if (libc_reserve == 0 || libc_reserve >= heap_bytes) {
        return heap_limit;
    }
    return (void *)(limit - libc_reserve);
}

static int do_memory_server(void *param, uint32_t pid)
{
    int ret;
    (void)param;
    (void)pid;
    noza_msg_t msg;

    void *service_heap_limit = memory_service_heap_limit();
    ta_init(&tinyalloc, heap_end, service_heap_limit, 256, 16, 8);
    heap_end = service_heap_limit;

    static uint32_t memory_service_id;
    int lookup_ret = name_lookup_register(NOZA_MEMORY_SERVICE_NAME, &memory_service_id);
    if (lookup_ret != NAME_LOOKUP_OK) {
        printk("memory: name register failed (%d)\n", lookup_ret);
    }

    for (;;) {
        if ((ret = noza_recv(&msg)) == 0) { // the pid in msg is the sender pid
			mem_msg_t *mem_msg = (mem_msg_t *)msg.ptr;
			// process the request
			switch (mem_msg->cmd) {
				case MEMORY_MALLOC:
                    mem_msg->ptr = ta_alloc(&tinyalloc, mem_msg->size);
                    if (mem_msg->ptr == NULL) {
                        mem_msg->code = ENOMEM;
                    } else {
                        mem_msg->code = MEMORY_SUCCESS;
                    }
					break;

				case MEMORY_FREE:
                    ta_free(&tinyalloc, mem_msg->ptr);
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
    (void)param;
    (void)pid;
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_memory_server, memory_server_stack, sizeof(memory_server_stack));
}
