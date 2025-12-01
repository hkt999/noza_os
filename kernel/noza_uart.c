#include "noza_uart.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

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

void noza_uart_init(void)
{
    uart_init(NOZA_UART_PORT, NOZA_UART_BAUD);
    uart_set_format(NOZA_UART_PORT, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(NOZA_UART_PORT, false, false);
    uart_set_fifo_enabled(NOZA_UART_PORT, true);
    gpio_set_function(NOZA_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(NOZA_UART_RX_PIN, GPIO_FUNC_UART);
}

void noza_uart_enable_rx_irq(void)
{
    uart_set_irq_enables(NOZA_UART_PORT, true, false);
}

bool noza_uart_readable(void)
{
    return uart_is_readable(NOZA_UART_PORT);
}

void noza_uart_putc(char c)
{
    while (!uart_is_writable(NOZA_UART_PORT)) {
        tight_loop_contents();
    }
    uart_putc_raw(NOZA_UART_PORT, c);
}

void noza_uart_write(const char *buf, size_t len)
{
    if (buf == NULL) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            noza_uart_putc('\r');
        }
        noza_uart_putc(buf[i]);
    }
}

int noza_getchar_timeout_us(uint32_t timeout_us)
{
    if (timeout_us == 0) {
        if (!uart_is_readable(NOZA_UART_PORT)) {
            return -1;
        }
        return (int)uart_getc(NOZA_UART_PORT);
    }

    absolute_time_t deadline = delayed_by_us(get_absolute_time(), timeout_us);
    while (!uart_is_readable(NOZA_UART_PORT)) {
        if (absolute_time_diff_us(deadline, get_absolute_time()) <= 0) {
            return -1;
        }
        tight_loop_contents();
    }
    return (int)uart_getc(NOZA_UART_PORT);
}

int noza_getchar(void)
{
    while (!uart_is_readable(NOZA_UART_PORT)) {
        tight_loop_contents();
    }
    return (int)uart_getc(NOZA_UART_PORT);
}
