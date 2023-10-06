#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "nozaos.h"
#include "kernel/syscall.h"

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

typedef struct boot_info {
	void (*user_entry)(void *param, uint32_t pid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t created;
} boot_info_t;

void free_stack(void *ptr)
{
	free(ptr);
}

extern void app_run(boot_info_t *info);

#define MAX_SERVICES    8
static void *service_entry[MAX_SERVICES];
static int service_count = 0;
void noza_add_service(void (*entry)(void *param, uint32_t pid))
{
    if (service_count >= MAX_SERVICES) {
        printf("fatal: noza_add_service: too many services\n");
		return;
    }
    service_entry[service_count++] = entry;
}

void noza_run_services()
{
    // initial all registered service
    for (int i = 0; i < service_count; i++) {
        noza_thread_create(service_entry[i], NULL, 0);
    }
}

int noza_thread_create_with_stack(void (*entry)(void *, uint32_t pid), void *param, uint32_t priority, uint8_t *user_stack, uint32_t size)
{
	volatile boot_info_t boot_info;
	boot_info.user_entry = entry;
	boot_info.user_param = param;
	boot_info.stack_ptr = (uint32_t *)user_stack;
	boot_info.stack_size = size;
	boot_info.created = 0;
    int ret =  noza_syscall(NSC_THREAD_CREATE, (uint32_t) app_run, (uint32_t) &boot_info, priority);
	while (boot_info.created==0) {
		noza_thread_yield();
	}

	return ret;
}

#define DEFAULT_STACK_SIZE 4096
int noza_thread_create(void (*entry)(void *, uint32_t), void *param, uint32_t priority)
{
	uint8_t *stack_ptr = (uint8_t *)malloc(DEFAULT_STACK_SIZE);
	uint32_t stack_size = DEFAULT_STACK_SIZE;
	return noza_thread_create_with_stack(entry, param, priority, stack_ptr, stack_size);
}

