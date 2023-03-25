#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "syslib.h"
#include "kernel/syscall.h"

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

typedef struct boot_info {
	void (*user_entry)(void *param);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t created;
} boot_info_t;

void free_stack(void *ptr)
{
	free(ptr);
}

extern void app_bootstrap(boot_info_t *info);
int noza_thread_create(void (*entry)(void *), void *param, uint32_t priority)
{
	volatile boot_info_t boot_info;
	boot_info.user_entry = entry;
	boot_info.user_param = param;
	boot_info.stack_ptr = (uint32_t *)malloc(4096); // TODO: use noza memory allocator
	boot_info.stack_size = 4096;
	boot_info.created = 0;
	printf("stack_ptr: %p\n", boot_info.stack_ptr);
    int ret =  noza_syscall(NSC_THREAD_CREATE, (uint32_t) app_bootstrap, (uint32_t) &boot_info, priority);
	while (boot_info.created==0) {
		noza_thread_yield();
	}

	return ret;
}
