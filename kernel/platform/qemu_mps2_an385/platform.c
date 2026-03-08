#include "noza_config.h"
#include "platform.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#include "board_config.h"
#include "noza_uart.h"

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#define UART0_DATA      REG32(QEMU_MPS2_UART0_BASE + 0x000u)
#define UART0_STATE     REG32(QEMU_MPS2_UART0_BASE + 0x004u)
#define UART0_CTRL      REG32(QEMU_MPS2_UART0_BASE + 0x008u)
#define UART0_BAUDDIV   REG32(QEMU_MPS2_UART0_BASE + 0x010u)

#define TIMER1_LOAD     REG32(QEMU_MPS2_DUALTIMER0_BASE + 0x000u)
#define TIMER1_VALUE    REG32(QEMU_MPS2_DUALTIMER0_BASE + 0x004u)
#define TIMER1_CONTROL  REG32(QEMU_MPS2_DUALTIMER0_BASE + 0x008u)
#define TIMER1_INTCLR   REG32(QEMU_MPS2_DUALTIMER0_BASE + 0x00Cu)
#define TIMER1_BGLOAD   REG32(QEMU_MPS2_DUALTIMER0_BASE + 0x018u)

#define SYST_CSR        REG32(QEMU_MPS2_SYST_CSR)
#define SYST_RVR        REG32(QEMU_MPS2_SYST_RVR)
#define SYST_CVR        REG32(QEMU_MPS2_SYST_CVR)
#define SCB_ICSR        REG32(QEMU_MPS2_SCB_ICSR)
#define SCB_SHPR2       REG32(QEMU_MPS2_SCB_SHPR2)
#define SCB_SHPR3       REG32(QEMU_MPS2_SCB_SHPR3)

static uint32_t interrupt_state[NOZA_OS_NUM_CORES];
static uint64_t monotonic_ticks;
static uint32_t timer_last_value;
static uint32_t random_state;

static void qemu_timer_init(void)
{
    TIMER1_CONTROL = 0;
    TIMER1_INTCLR = 1;
    TIMER1_LOAD = 0xFFFFFFFFu;
    TIMER1_BGLOAD = 0xFFFFFFFFu;
    TIMER1_CONTROL = SP804_CTRL_ENABLE | SP804_CTRL_32BIT | SP804_CTRL_PRESCALE_DIV1;
    timer_last_value = TIMER1_VALUE;
}

static uint64_t qemu_timer_update_ticks(void)
{
    uint32_t current = TIMER1_VALUE;
    uint32_t delta = timer_last_value - current;
    timer_last_value = current;
    monotonic_ticks += (uint64_t)delta;
    return monotonic_ticks;
}

void platform_uart_init(void)
{
    UART0_CTRL = 0;
    UART0_BAUDDIV = (QEMU_MPS2_SYSCLK_HZ + (115200u / 2u)) / 115200u;
    UART0_CTRL = CMSDK_UART_CTRL_TX_EN | CMSDK_UART_CTRL_RX_EN;
}

void platform_uart_enable_rx_irq(void)
{
}

bool platform_uart_readable(void)
{
    return (UART0_STATE & CMSDK_UART_STATE_RXBF) != 0;
}

void platform_uart_putc(char c)
{
    while ((UART0_STATE & CMSDK_UART_STATE_TXBF) != 0) {
    }
    UART0_DATA = (uint32_t)(uint8_t)c;
}

int platform_uart_getc(void)
{
    while (!platform_uart_readable()) {
    }
    return (int)(UART0_DATA & 0xFFu);
}

uint32_t platform_interrupt_disable(void)
{
    uint32_t flags;
    __asm volatile(
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r"(flags)
        :
        : "memory");
    return flags;
}

void platform_interrupt_restore(uint32_t flags)
{
    __asm volatile("msr primask, %0" : : "r"(flags) : "memory");
}

void platform_trigger_pendsv(void)
{
    SCB_ICSR = QEMU_MPS2_ICSR_PENDSVSET;
    __asm volatile("dsb" : : : "memory");
    __asm volatile("isb" : : : "memory");
}

int64_t platform_get_absolute_time_us(void)
{
    uint32_t flags = platform_interrupt_disable();
    uint64_t ticks = qemu_timer_update_ticks();
    platform_interrupt_restore(flags);
    return (int64_t)(ticks / (uint64_t)(QEMU_MPS2_SYSCLK_HZ / 1000000u));
}

int platform_uart_getchar_timeout_us(uint32_t timeout_us)
{
    if (timeout_us == 0) {
        if (!platform_uart_readable()) {
            return -1;
        }
        return (int)(UART0_DATA & 0xFFu);
    }

    int64_t deadline = platform_get_absolute_time_us() + timeout_us;
    while (!platform_uart_readable()) {
        if (platform_get_absolute_time_us() >= deadline) {
            return -1;
        }
    }
    return (int)(UART0_DATA & 0xFFu);
}

void platform_io_init(void)
{
    for (uint32_t i = 0; i < NOZA_OS_NUM_CORES; i++) {
        interrupt_state[i] = 0;
    }

    qemu_timer_init();
    platform_uart_init();
    noza_uart_write("[noza] qemu mps2 uart init\n", 28);

    SCB_SHPR2 = (SCB_SHPR2 & 0x00FFFFFFu) | (0xFFu << 24);
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFu) | (0xFFu << 16) | (0xFFu << 24);
    random_state = (uint32_t)platform_get_absolute_time_us() ^ 0xA5A55A5Au;
}

uint32_t platform_get_running_core(void)
{
    return 0;
}

void platform_multicore_init(void (*noza_os_scheduler)(void))
{
    (void)noza_os_scheduler;
}

void platform_request_schedule(int target_core)
{
    (void)target_core;
}

void platform_systick_config(unsigned int n)
{
    SYST_CSR = 0;
    __asm volatile("dsb" : : : "memory");
    __asm volatile("isb" : : : "memory");
    SCB_ICSR = QEMU_MPS2_ICSR_PENDSTCLR;

    if (n == 0) {
        return;
    }

    uint64_t ticks = (uint64_t)n * (uint64_t)(QEMU_MPS2_SYSCLK_HZ / 1000000u);
    if (ticks == 0) {
        ticks = 1;
    }
    if (ticks > 0x00FFFFFFu) {
        ticks = 0x00FFFFFFu;
    }

    SYST_RVR = (uint32_t)(ticks - 1u);
    SYST_CVR = 0;
    SYST_CSR = 0x7;
}

void platform_tick_cores(void)
{
}

void platform_idle(void)
{
    for (;;) {
        __asm volatile("wfi");
    }
}

void platform_panic(const char *msg, ...)
{
    char buf[256];
    va_list args;
    va_start(args, msg);
    int n = vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);
    if (n > 0) {
        size_t len = (n < (int)sizeof(buf)) ? (size_t)n : sizeof(buf) - 1u;
        noza_uart_write(buf, len);
    }
    for (;;) {
        __asm volatile("wfi");
    }
}

void platform_os_lock_init(void)
{
}

void platform_os_lock(uint32_t core)
{
    interrupt_state[core] = platform_interrupt_disable();
}

void platform_os_unlock(uint32_t core)
{
    platform_interrupt_restore(interrupt_state[core]);
}

uint32_t platform_get_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    if (random_state == 0) {
        random_state = 0x13579BDFu;
    }
    return random_state;
}

void platform_irq_init(void)
{
}

void platform_irq_mask(uint32_t irq_id)
{
    (void)irq_id;
}

void platform_irq_unmask(uint32_t irq_id)
{
    (void)irq_id;
}
