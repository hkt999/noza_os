#pragma once
#include <stdint.h>

int32_t usleep(uint32_t us);
uint32_t thread_create(void (*entry)(void *param), void *param, uint32_t pri);
uint32_t thread_yield();