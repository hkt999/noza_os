#pragma once

#include <stdint.h>

typedef struct spinlock_s {
    int num;
    volatile uint32_t *spinlock;
    int lock_thread;
} spinlock_t;

int noza_spinlock_init(spinlock_t *spinlock);
int noza_spinlock_free(spinlock_t *spinlock);
int noza_spinlock_lock(spinlock_t *spinlock);
int noza_spinlock_trylock(spinlock_t *spinlock);
int noza_spinlock_unlock(spinlock_t *spinlock);
int noza_raw_lock(spinlock_t *spinlock);