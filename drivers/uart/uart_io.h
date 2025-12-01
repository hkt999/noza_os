#pragma once

#include "noza_console_api.h"

int uart_service_start(void *param, uint32_t pid);
void uart_output_line(const char *line);
