#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Primitive UART0 helpers for early I/O and console paths.
// Blocking getchar for shell loops.
int noza_getchar(void);
// Non-blocking read; returns -1 when no data within timeout_us.
int noza_getchar_timeout_us(uint32_t timeout_us);

// Low-level UART initialization and I/O.
void noza_uart_init(void);
void noza_uart_enable_rx_irq(void);
void noza_uart_putc(char c);
void noza_uart_write(const char *buf, size_t len);
bool noza_uart_readable(void);
