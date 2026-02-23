#include <stdio.h>
#include <stdbool.h>
#include "spinlock.h"
#include "nozaos.h"
#include "posix/errno.h"

static inline void spinlock_wait(spinlock_t *spinlock)
{
    for (;;) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&spinlock->state, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return;
        }
        noza_thread_sleep_us(0, NULL);
    }
}

int noza_spinlock_init_debug(spinlock_t *spinlock, const char *file, int line)
{
    spinlock->state = 0;
    spinlock->lock_thread = -1;
    spinlock->owner_file = file;
    spinlock->owner_line = line;
    return 0;
}

int noza_spinlock_free(spinlock_t *spinlock)
{
    spinlock->state = 0;
    spinlock->lock_thread = -1;
    spinlock->owner_file = NULL;
    spinlock->owner_line = 0;
    return 0; // success
}

int noza_raw_lock(spinlock_t *spinlock)
{
    uint32_t tid = 0;
    spinlock_wait(spinlock);
    if (noza_thread_self(&tid) == 0) {
        spinlock->lock_thread = (int)tid;
    } else {
        spinlock->lock_thread = -1;
    }
    return 0;
}

int noza_spinlock_lock(spinlock_t *spinlock)
{
    uint32_t tid = 0;
    int has_tid = (noza_thread_self(&tid) == 0);
    if (has_tid && spinlock->lock_thread == (int)tid)
        return EDEADLK; // deadlock

    spinlock_wait(spinlock);
    spinlock->lock_thread = has_tid ? (int)tid : -1;

    return 0;
}

int noza_spinlock_trylock(spinlock_t *spinlock)
{
    uint32_t tid = 0;
    int has_tid = (noza_thread_self(&tid) == 0);
    if (has_tid && spinlock->lock_thread == (int)tid)
        return EDEADLK; // deadlock

    int expected = 0;
    if (!__atomic_compare_exchange_n(&spinlock->state, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return EBUSY;
    }
    spinlock->lock_thread = has_tid ? (int)tid : -1;
    return 0;
}

int noza_spinlock_unlock(spinlock_t *spinlock)
{
    uint32_t tid = 0;
    int has_tid = (noza_thread_self(&tid) == 0);
    if (__atomic_load_n(&spinlock->state, __ATOMIC_RELAXED) == 0) {
        return EPERM;
    }
    if (has_tid && spinlock->lock_thread != -1 && spinlock->lock_thread != (int)tid) {
        return EPERM;
    }
    spinlock->lock_thread = -1;
    __atomic_store_n(&spinlock->state, 0, __ATOMIC_RELEASE);
    return 0;
}
