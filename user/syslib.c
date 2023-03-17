#include "syslib.h"
#include "kernel/syscall.h"

extern void asm_noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
uint32_t noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2)
{
	uint32_t ret_addr = 0;
	asm_noza_syscall(r0, r1, r2, (uint32_t)&ret_addr); // the value will be filled into ret_addr

	return ret_addr; // so just return
}

// exported lib functions
int32_t usleep(uint32_t us)
{
	return noza_syscall(NSC_SLEEP, us/1000, 0);
}

uint32_t thread_create(void (*entry)(void *param), void *param, uint32_t pri)
{
    return noza_syscall(NSC_CREATE_THREAD, (uint32_t) entry, (uint32_t) param);
}

uint32_t thread_yield()
{
	return noza_syscall(NSC_YIELD, 0, 0);
}