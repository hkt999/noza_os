#include "noza_config.h"
#include "platform.h"

#include <stdbool.h>
#include <stdarg.h>

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/m0plus.h"
#include "hardware/regs/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#if NOZA_OS_NUM_CORES > 1
#include "pico/multicore.h"
#endif

#include "noza_uart.h"
#if NOZA_OS_ENABLE_IRQ
#include "noza_irq_defs.h"
extern void noza_irq_handle_from_isr(uint32_t irq_id);
static void rp2040_common_irq_handler(void);
#endif

#ifndef NOZA_UART_PORT
#define NOZA_UART_PORT uart0
#endif

#ifndef NOZA_UART_BAUD
#define NOZA_UART_BAUD 115200
#endif

#ifndef NOZA_UART_TX_PIN
#define NOZA_UART_TX_PIN PICO_DEFAULT_UART_TX_PIN
#endif

#ifndef NOZA_UART_RX_PIN
#define NOZA_UART_RX_PIN PICO_DEFAULT_UART_RX_PIN
#endif

void platform_uart_init(void)
{
    uart_init(NOZA_UART_PORT, NOZA_UART_BAUD);
    uart_set_format(NOZA_UART_PORT, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(NOZA_UART_PORT, false, false);
    uart_set_fifo_enabled(NOZA_UART_PORT, true);
    gpio_set_function(NOZA_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(NOZA_UART_RX_PIN, GPIO_FUNC_UART);
}

void platform_uart_enable_rx_irq(void)
{
    uart_set_irq_enables(NOZA_UART_PORT, true, false);
}

bool platform_uart_readable(void)
{
    return uart_is_readable(NOZA_UART_PORT);
}

void platform_uart_putc(char c)
{
    while (!uart_is_writable(NOZA_UART_PORT)) {
        tight_loop_contents();
    }
    uart_putc_raw(NOZA_UART_PORT, c);
}

int platform_uart_getc(void)
{
    while (!platform_uart_readable()) {
        tight_loop_contents();
    }
    return (int)uart_getc(NOZA_UART_PORT);
}

int platform_uart_getchar_timeout_us(uint32_t timeout_us)
{
    if (timeout_us == 0) {
        if (!platform_uart_readable()) {
            return -1;
        }
        return (int)uart_getc(NOZA_UART_PORT);
    }

    absolute_time_t deadline = delayed_by_us(get_absolute_time(), timeout_us);
    while (!platform_uart_readable()) {
        if (absolute_time_diff_us(deadline, get_absolute_time()) <= 0) {
            return -1;
        }
        tight_loop_contents();
    }
    return (int)uart_getc(NOZA_UART_PORT);
}

void platform_io_init(void)
{
    platform_uart_init();
    noza_uart_write("[noza] uart init\n", 17);

    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS);
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS);
}

uint32_t platform_get_running_core(void)
{
    return get_core_num();
}

uint32_t platform_interrupt_disable(void)
{
    return save_and_disable_interrupts();
}

void platform_interrupt_restore(uint32_t flags)
{
    restore_interrupts(flags);
}

void platform_trigger_pendsv(void)
{
    scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS;
}

#if NOZA_OS_NUM_CORES > 1
static volatile bool core_irq_ready[NOZA_OS_NUM_CORES];

static void multicore_irq_common_handler(void)
{
    while (multicore_fifo_rvalid()) {
        multicore_fifo_pop_blocking();
    }
    multicore_fifo_clear_irq();
    platform_trigger_pendsv();
}

static void core0_interrupt_handler(void)
{
    multicore_irq_common_handler();
}

static void core1_interrupt_handler(void)
{
    multicore_irq_common_handler();
}
#endif

void platform_multicore_init(void (*noza_os_scheduler)(void))
{
#if NOZA_OS_NUM_CORES > 1
    if (get_core_num() == 0) {
        multicore_fifo_clear_irq();
        multicore_launch_core1(noza_os_scheduler);
        irq_set_exclusive_handler(SIO_IRQ_PROC0, core0_interrupt_handler);
        irq_set_priority(SIO_IRQ_PROC0, 32);
        irq_set_enabled(SIO_IRQ_PROC0, true);
        core_irq_ready[0] = true;
    } else {
        multicore_fifo_clear_irq();
        irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
        irq_set_priority(SIO_IRQ_PROC1, 32);
        irq_set_enabled(SIO_IRQ_PROC1, true);
        core_irq_ready[1] = true;
    }
#else
    (void)noza_os_scheduler;
#endif
}

int64_t platform_get_absolute_time_us(void)
{
    return to_us_since_boot(get_absolute_time());
}

void platform_systick_config(unsigned int n)
{
    systick_hw->csr = 0;
    __dsb();
    __isb();

    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET), M0PLUS_ICSR_PENDSTCLR_BITS);

    if (n == 0) {
        return;
    }
    if (n > 0x01000000u) {
        n = 0x01000000u;
    }

    systick_hw->rvr = n - 1UL;
    systick_hw->cvr = 0;
    systick_hw->csr = 0x03;
}

void platform_request_schedule(int target_core)
{
#if NOZA_OS_NUM_CORES > 1
    int current = get_core_num();
    if (target_core < 0 || target_core >= NOZA_OS_NUM_CORES) {
        return;
    }
    if (target_core == current || !core_irq_ready[target_core]) {
        return;
    }
    multicore_fifo_push_timeout_us(0, 0);
#else
    (void)target_core;
#endif
}

void platform_tick_cores(void)
{
#if NOZA_OS_NUM_CORES > 1
    int current = get_core_num();
    for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
        if (core != current) {
            platform_request_schedule(core);
        }
    }
#endif
}

void platform_idle(void)
{
    for (;;) {
        __wfi();
    }
}

void platform_panic(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    panic(msg, args);
    va_end(args);
}

static uint32_t interrupt_state[NOZA_OS_NUM_CORES];
static volatile noza_spin_lock_t *spinlock;

void platform_os_lock_init(void)
{
    for (int i = 0; i < NOZA_OS_NUM_CORES; i++) {
        interrupt_state[i] = 0;
    }
    int spinlock_num_count = spin_lock_claim_unused(true);
    spinlock = spin_lock_init(spinlock_num_count);
}

void platform_os_lock(uint32_t core)
{
    interrupt_state[core] = platform_interrupt_disable();
    spin_lock_unsafe_blocking(spinlock);
}

void platform_os_unlock(uint32_t core)
{
    spin_unlock_unsafe(spinlock);
    platform_interrupt_restore(interrupt_state[core]);
}

uint32_t platform_get_random(void)
{
    uint32_t random = 0;
    volatile uint32_t *rnd_reg = (uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);

    for (uint32_t k = 0; k < 32; k++) {
        random <<= 1;
        random += (0x00000001u & (*rnd_reg));
    }

    return random;
}

void platform_irq_init(void)
{
#if NOZA_OS_ENABLE_IRQ
    irq_set_exclusive_handler(UART0_IRQ, rp2040_common_irq_handler);
    irq_set_enabled(UART0_IRQ, true);
#endif
}

void platform_irq_mask(uint32_t irq_id)
{
#if NOZA_OS_ENABLE_IRQ
    if (irq_id < NOZA_PLATFORM_IRQ_COUNT) {
        irq_set_enabled(irq_id, false);
    }
#else
    (void)irq_id;
#endif
}

void platform_irq_unmask(uint32_t irq_id)
{
#if NOZA_OS_ENABLE_IRQ
    if (irq_id < NOZA_PLATFORM_IRQ_COUNT) {
        irq_set_enabled(irq_id, true);
    }
#else
    (void)irq_id;
#endif
}

#if NOZA_OS_ENABLE_IRQ
static void rp2040_common_irq_handler(void)
{
    uint32_t vector = scb_hw->icsr & M0PLUS_ICSR_VECTACTIVE_BITS;
    if (vector < 16) {
        return;
    }

    uint32_t irq_id = vector - 16;
    if (irq_id >= NOZA_PLATFORM_IRQ_COUNT) {
        return;
    }
    noza_irq_handle_from_isr(irq_id);
}
#endif
