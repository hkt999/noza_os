#pragma once
#include <stdint.h>

void noza_application_entry(void (*entry)(void *param), void *param, uint32_t pri);
void noza_start();
uint32_t noza_read_syscall_result();
