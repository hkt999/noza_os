// Seperate functions from noza_os.c, in order to abstrate the platform.
#include "../noza_config.h"
#include "../platform.h"
#include "pico/stdlib.h"
#include "hardware/irq.h"
#if NOZA_OS_NUM_CORES > 1
#include "pico/multicore.h"
#endif
#include "hardware/structs/systick.h"
#include "hardware/structs/scb.h" 
#include "hardware/regs/m0plus.h"
#include "hardware/sync.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/regs/rosc.h"
#include "hardware/regs/addressmap.h"
#include <stdbool.h>

void platform_io_init()
{
    stdio_init_all();
    // set the SVC interrupt priority
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS); // setup as priority 3 (2 bits)
    // set the PENDSV interrupt priority
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS); // setup as priority 3 (2 bits)
}

uint32_t platform_get_running_core()
{
    return get_core_num();
}

#if NOZA_OS_NUM_CORES > 1
static volatile bool core_irq_ready[NOZA_OS_NUM_CORES];
static void multicore_irq_common_handler(void)
{
    while (multicore_fifo_rvalid()) {
        multicore_fifo_pop_blocking();
    }
    multicore_fifo_clear_irq(); // clear interrupt
    scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS; // issue PendSV interrupt
}

static void core0_interrupt_handler()
{
    multicore_irq_common_handler();
}

static void core1_interrupt_handler()
{
    multicore_irq_common_handler();
}
#endif

void platform_multicore_init(void (*noza_os_scheduler)(void))
{
#if NOZA_OS_NUM_CORES > 1
    if (get_core_num() == 0) {
        multicore_fifo_clear_irq();
        multicore_launch_core1(noza_os_scheduler); // launch core1 to run noza_os_scheduler
        irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_interrupt_handler);
        irq_set_priority(SIO_IRQ_PROC0, 32);
        irq_set_enabled(SIO_IRQ_PROC0, true);
        core_irq_ready[0] = true;
    } else {
        multicore_fifo_clear_irq();
        irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
        irq_set_priority(SIO_IRQ_PROC1, 32);
        irq_set_enabled(SIO_IRQ_PROC1, true); // enable the FIFO interrupt
        core_irq_ready[1] = true;
    }
#endif
}

int64_t platform_get_absolute_time_us()
{
    return to_us_since_boot(get_absolute_time());
}

void platform_systick_config(unsigned int n)
{
    // stop systick and cancel it if it is pending
    systick_hw->csr = 0;    // disable timer and irq 
    __dsb();                // make sure systick is disabled
    __isb();                // and it is really off

    // clear the systick exception pending bit if it got set
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET), M0PLUS_ICSR_PENDSTCLR_BITS);

    systick_hw->rvr = (n) - 1UL;    // set the reload value
    systick_hw->cvr = 0;    // clear counter to force reload
    systick_hw->csr = 0x03; // arm irq, start counter with 1 usec clock
}

void platform_request_schedule(int target_core)
{
#if NOZA_OS_NUM_CORES > 1
    int current = get_core_num();
    if (target_core < 0 || target_core >= NOZA_OS_NUM_CORES)
        return;
    if (target_core == current)
        return;
    if (!core_irq_ready[target_core])
        return;
    multicore_fifo_push_timeout_us(0, 0);
#else
    (void)target_core;
#endif
}

void platform_tick_cores()
{
#if NOZA_OS_NUM_CORES > 1
    int current = get_core_num();
    for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
        if (core == current)
            continue;
        platform_request_schedule(core);
    }
#endif
}

void platform_idle()
{
    for (;;) {
        __wfi(); // idle, power saving
    }
}

uint32_t test_addr;

#include <stdio.h>
#include <stdarg.h>
void platform_panic(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    panic(msg, args);
    va_end(args);
}

static uint32_t        interrupt_state[NOZA_OS_NUM_CORES];  // array of interrupt states for each core
static int             spinlock_num_count;                  // count of spinlocks
static volatile noza_spin_lock_t     *spinlock;                     // pointer to spinlock count

void platform_os_lock_init() {
    for (int i=0; i<NOZA_OS_NUM_CORES; i++)
        interrupt_state[i] = 0;
    int spinlock_num_count = spin_lock_claim_unused(true);
    spinlock = spin_lock_init(spinlock_num_count); // init spin lock
}

void platform_os_lock(uint32_t core) {
    interrupt_state[core] = save_and_disable_interrupts(); // save current core
    spin_lock_unsafe_blocking(spinlock);
}

void platform_os_unlock(uint32_t core) {
    spin_unlock_unsafe(spinlock);
    restore_interrupts(interrupt_state[core]);
}

uint32_t platform_get_random(void)
{
    uint32_t k, random=0;
    volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    
    for (k=0; k<32; k++) {
        random = random << 1;
        random=random + (0x00000001 & (*rnd_reg));
    }

    return random;
}
