#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t state;
} kernel_trap_info_t;

typedef struct {
    uint32_t regs[8]; // r0, r1, r2, r3, r12, lr, pc, xpsr
    uint32_t sp;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint8_t valid;
} arch_fault_snapshot_t;

uint32_t *arch_build_stack(uint32_t thread_id, uint32_t *stack, uint32_t size,
    void (*entry)(void *), void *param);
void arch_trap(void *stack_ptr, kernel_trap_info_t *info);
void arch_core_dump(void *stack_ptr, uint32_t pid);
void arch_fault_capture(uint32_t *psp);
bool arch_get_fault_snapshot(arch_fault_snapshot_t *out);
void arch_init_kernel_stack(uint32_t *stack);
uint32_t *arch_resume_thread(uint32_t *stack);
