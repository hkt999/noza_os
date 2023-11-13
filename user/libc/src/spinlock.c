#include "spinlock.h"
#include "pico.h"
#include "hardware/address_mapped.h"
#include "hardware/sync.h"
#include "hardware/regs/sio.h"
#include "nozaos.h"
#include "posix/errno.h"

// hardware spinlock interface
int noza_spinlock_init(spinlock_t *spinlock) {
    spinlock->num = spin_lock_claim_unused(true);
    spinlock->spinlock = spin_lock_init(spinlock->num); // init spin lock
    spinlock->lock_thread = -1;
    return 0; // success
}

int noza_spinlock_free(spinlock_t *spinlock) {
    spin_lock_unclaim(spinlock->num);
    return 0; // success
}

int noza_raw_lock(spinlock_t *spinlock) {
    while (is_spin_locked(spinlock->spinlock)) {
        noza_thread_sleep_us(0, NULL); // yield
    }
    spin_lock_unsafe_blocking (spinlock->spinlock);

    return 0;
}

int noza_spinlock_lock(spinlock_t *spinlock) {
    uint32_t tid;
    noza_thread_self(&tid); 
    if (spinlock->lock_thread == tid)
        return EDEADLK; // deadlock
    noza_raw_lock(spinlock);
    spinlock->lock_thread = tid;

    return 0;
}

int noza_spinlock_trylock(spinlock_t *spinlock) {
    uint32_t tid;
    noza_thread_self(&tid);
    if (spinlock->lock_thread == tid)
        return EDEADLK; // deadlock

    if (is_spin_locked(spinlock->spinlock))
        return EBUSY; // already locked

    spin_lock_unsafe_blocking(spinlock->spinlock); // acquire lock
    spinlock->lock_thread = tid;
    return 0;
}

int noza_spinlock_unlock(spinlock_t *spinlock) {
    spin_unlock_unsafe(spinlock->spinlock);
    spinlock->lock_thread = -1;
    return 0;
}