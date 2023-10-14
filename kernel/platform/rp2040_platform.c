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
static void core1_interrupt_handler() {

    while (multicore_fifo_rvalid()){
        multicore_fifo_pop_blocking();
    }
    multicore_fifo_clear_irq(); // clear interrupt

    scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS; // issue PendSV interrupt
}
#endif

void platform_multicore_init(void (*noza_os_scheduler)(void))
{
#if NOZA_OS_NUM_CORES > 1
    if (get_core_num() == 0) { 
        multicore_launch_core1(noza_os_scheduler); // launch core1 to run noza_os_scheduler
    } else {
        multicore_fifo_clear_irq();
        irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
        // set priority to 32, priority higher then pendSV interrupt
        // hardware interrupt priority is only 2 bits, the svc and pensv are both seting to 0x3
        irq_set_priority(SIO_IRQ_PROC1, 32);
        irq_set_enabled(SIO_IRQ_PROC1, true); // enable the FIFO interrupt
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

void platform_tick(uint32_t ms)
{
#if NOZA_OS_NUM_CORES > 1
    if (get_core_num()==0) {
        multicore_fifo_push_blocking(0); // tick core1
        platform_systick_config(ms);
    }
#endif
}

void platform_idle()
{
    for (;;) {
        __wfi(); // idle, power saving
    }
}

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