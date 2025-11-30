#pragma once

#include <stdint.h>
#include <stdbool.h>

// abstraction interface
typedef uint32_t noza_spin_lock_t;
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
    uint8_t  valid;
} platform_fault_snapshot_t;

void        platform_io_init();
uint32_t    platform_get_running_core();
void        platform_multicore_init(void (*noza_os_scheduler)(void));
void        platform_request_schedule(int target_core);
int64_t     platform_get_absolute_time_us();
uint32_t    platform_get_random();
void        platform_systick_config(unsigned int n);
void        platform_tick_cores();
void        platform_idle();
void        platform_panic(const char *msg, ...);
void        platform_os_lock_init();
void        platform_os_lock(uint32_t core);
void        platform_os_unlock(uint32_t core);
void        platform_trap(void *_stack_ptr, kernel_trap_info_t *info);
void        platform_core_dump(void *stack_ptr, uint32_t pid);
void        platform_fault_capture(uint32_t *psp);
bool        platform_get_fault_snapshot(platform_fault_snapshot_t *out);
void        platform_irq_init();
void        platform_irq_mask(uint32_t irq_id);
void        platform_irq_unmask(uint32_t irq_id);
