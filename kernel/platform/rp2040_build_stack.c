#include <stdint.h>
#include "../platform.h"

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12; // r12 is not saved by hardware, but by software (isr_svcall)
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr; // psr thumb bit
} interrupted_stack_t;

typedef struct {
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t lr;
} user_stack_t;

uint32_t *platform_build_stack(uint32_t thread_id, uint32_t *stack, uint32_t size, void (*entry)(void *), void *param)
{
    #define NOZA_OS_THREAD_PSP       0xFFFFFFFD  // exception return behavior (thread mode)
    // TODO: move this to platform specific code
    uint32_t *new_ptr = stack + size - 17; // end of task_stack
    user_stack_t *u = (user_stack_t *) new_ptr;
    u->lr = (uint32_t) NOZA_OS_THREAD_PSP; // return to thread mode, use PSP

    interrupted_stack_t *is = (interrupted_stack_t *)(new_ptr + (sizeof(user_stack_t)/sizeof(uint32_t)));
    is->pc = (uint32_t) entry;
    is->xpsr = (uint32_t) 0x01000000; // thumb bit
    is->r0 = (uint32_t) param;
    is->r1 = thread_id;

    return new_ptr;
}

#include <stdio.h>
extern uint32_t platform_get_running_core();
void platform_core_dump(void *_stack_ptr)
{
    uint32_t *stack_ptr = (uint32_t *)_stack_ptr;
    stack_ptr -= 17;
    user_stack_t *us = (user_stack_t *)stack_ptr;
    interrupted_stack_t *is = (interrupted_stack_t *)(stack_ptr + (sizeof(user_stack_t)/sizeof(uint32_t)));

    printf("core(%d) dump:\n", platform_get_running_core());
    printf("r0  %08x   r1 %08x  r2 %08x  r3   %08x\n", is->r0, is->r1, is->r2, is->r3);
    printf("r4  %08x   r5 %08x  r6 %08x  r7   %08x\n", us->r4, us->r5, us->r6, us->r7);
    printf("r8  %08x   r9 %08x r10 %08x  r11  %08x\n", us->r8, us->r9, us->r10, us->r11);
    printf("r12 %08x   lr %08x  pc %08x  xpsr %08x\n\n", is->r12, is->lr, is->pc, is->xpsr);
}

// copy register from info to interrupt stack (for resume to user thread)
void platform_trap(void *_stack_ptr, kernel_trap_info_t *info)
{
    uint32_t *stack_ptr = (uint32_t *)_stack_ptr;
    interrupted_stack_t *is = (interrupted_stack_t *)(stack_ptr + (sizeof(user_stack_t)/sizeof(uint32_t)));
    is->r0 = info->r0;
    is->r1 = info->r1;
    is->r2 = info->r2;
    is->r3 = info->r3;
}

#include "../syscall.h"
const char *syscall_name[] = {
    [NSC_THREAD_SLEEP] = "NSC_THREAD_SLEEP",
    [NSC_THREAD_KILL] = "NSC_THREAD_KILL",
    [NSC_THREAD_CREATE] = "NSC_THREAD_CREATE",
    [NSC_THREAD_CHANGE_PRIORITY] = "NSC_THREAD_CHANGE_PRIORITY",
    [NSC_THREAD_JOIN] = "NSC_THREAD_JOIN",
    [NSC_THREAD_DETACH] = "NSC_THREAD_DETACH",
    [NSC_THREAD_TERMINATE] = "NSC_THREAD_TERMINATE",
    [NSC_THREAD_SELF] = "NSC_THREAD_SELF",
    [NSC_RECV] = "NSC_RECV",
    [NSC_REPLY] = "NSC_REPLY",
    [NSC_CALL] = "NSC_CALL",
    [NSC_NB_RECV] = "NSC_NB_RECV",
    [NSC_NB_CALL] = "NSC_NB_CALL"
};

static const char *syscall_id_to_name(uint32_t id) 
{
    if (id>=NSC_NUM_SYSCALLS) {
        return "UNKNOWN";
    }
    return syscall_name[id];
}

void dump_interrupt_stack(uint32_t *stack_ptr, uint32_t callid, uint32_t pid)
{
    if (callid != NSC_THREAD_SLEEP) {
        interrupted_stack_t *is = (interrupted_stack_t *)(stack_ptr + (sizeof(user_stack_t)/sizeof(uint32_t)));
        printf("core:%d, stack (pid:%d) r0: %08x r1: %08x r2: %08x r3: %08x (%s:%d)\n",
            platform_get_running_core(), pid, is->r0, is->r1, is->r2, is->r3, syscall_id_to_name(callid), callid);
        //printf("r12 %08x   lr %08x  pc %08x  xpsr %08x\n\n", is->r12, is->lr, is->pc, is->xpsr);
    }
}