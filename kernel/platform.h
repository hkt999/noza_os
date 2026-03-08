#pragma once

#include <stdbool.h>
#include <stdint.h>

// abstraction interface
typedef uint32_t noza_spin_lock_t;

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
uint32_t    platform_interrupt_disable(void);
void        platform_interrupt_restore(uint32_t flags);
void        platform_trigger_pendsv(void);
void        platform_irq_init();
void        platform_irq_mask(uint32_t irq_id);
void        platform_irq_unmask(uint32_t irq_id);
void        platform_uart_init(void);
void        platform_uart_enable_rx_irq(void);
bool        platform_uart_readable(void);
void        platform_uart_putc(char c);
int         platform_uart_getc(void);
int         platform_uart_getchar_timeout_us(uint32_t timeout_us);
