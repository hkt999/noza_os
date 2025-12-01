#pragma once

#include <stdint.h>

int console_write(const char *buf, uint32_t len);
int console_readline(char *buf, uint32_t max_len, uint32_t *out_len);
