#pragma once

#include <stdarg.h>

// Low-level UART-backed printk for kernel and early boot.
// Safe to call before other subsystems are up; non-reentrant.
void printk(const char *fmt, ...);
void kvprintk(const char *fmt, va_list args);
