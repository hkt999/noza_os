#pragma once

#include <stdint.h>

typedef struct spinlock_s {
    volatile int state;
    int lock_thread;
    const char *owner_file;
    int owner_line;
} spinlock_t;

int noza_spinlock_init_debug(spinlock_t *spinlock, const char *file, int line);
#define noza_spinlock_init(lock) noza_spinlock_init_debug((lock), __FILE__, __LINE__)
int noza_spinlock_free(spinlock_t *spinlock);
int noza_spinlock_lock(spinlock_t *spinlock);
int noza_spinlock_trylock(spinlock_t *spinlock);
int noza_spinlock_unlock(spinlock_t *spinlock);
int noza_raw_lock(spinlock_t *spinlock);
