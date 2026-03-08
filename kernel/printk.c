#include <stdarg.h>
#include <stdio.h>
#include "printk.h"
#include "noza_uart.h"
#include "platform.h"

#ifndef PRINTK_BUF_SIZE
#define PRINTK_BUF_SIZE 256
#endif

void kvprintk(const char *fmt, va_list args)
{
    char buf[PRINTK_BUF_SIZE];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    if (n < 0) {
        return;
    }
    if ((size_t)n >= sizeof(buf)) {
        n = PRINTK_BUF_SIZE - 1;
        buf[n] = '\0';
    }

    uint32_t flags = platform_interrupt_disable();
    noza_uart_write(buf, (size_t)n);
    platform_interrupt_restore(flags);
}

void printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintk(fmt, args);
    va_end(args);
}
