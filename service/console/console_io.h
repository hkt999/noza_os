#pragma once

#include "noza_console_api.h"

int console_service_start(void *param, uint32_t pid);
void console_output_line(const char *line);
