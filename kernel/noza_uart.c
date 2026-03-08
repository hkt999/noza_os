#include "noza_uart.h"
#include "platform.h"

void noza_uart_init(void)
{
    platform_uart_init();
}

void noza_uart_enable_rx_irq(void)
{
    platform_uart_enable_rx_irq();
}

bool noza_uart_readable(void)
{
    return platform_uart_readable();
}

void noza_uart_putc(char c)
{
    platform_uart_putc(c);
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
    return platform_uart_getchar_timeout_us(timeout_us);
}

int noza_getchar(void)
{
    return platform_uart_getc();
}
