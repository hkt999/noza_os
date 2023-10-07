#pragma once

#include <stdint.h>

// abstraction interface
typedef uint32_t noza_spin_lock_t;

void        platform_io_init();
uint32_t    platform_get_running_core();
void        platform_multicore_init(void (*noza_os_scheduler)(void));
uint32_t    platform_get_absolute_time();
uint32_t    platform_get_random();
void        platform_systick_config(unsigned int n);
void        platform_tick(uint32_t ms);
void        platform_idle();
void        platform_panic(const char *msg, ...);
void        platform_os_lock_init();
void        platform_os_lock(uint32_t core);
void        platform_os_unlock(uint32_t core);

