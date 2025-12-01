#include <stdarg.h>
#include <stdio.h>
#include "printk.h"

#include "hardware/sync.h"
#include "noza_uart.h"

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

    uint32_t flags = save_and_disable_interrupts();
    noza_uart_write(buf, (size_t)n);
    restore_interrupts(flags);
}

void printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintk(fmt, args);
    va_end(args);
}
