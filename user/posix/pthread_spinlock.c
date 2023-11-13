#include "spinlock.h"

typedef spinlock_t nz_pthread_spinlock_t;
int nz_pthread_spin_init(nz_pthread_spinlock_t *lock, int pshared)
{
    /* TODO: consider pshared
        NZ_PTHREAD_PROCESS_SHARED
        NZ_PTHREAD_PROCESS_PRIVATE
    */
    return noza_spinlock_init(lock);
}

int nz_pthread_spin_destroy(nz_pthread_spinlock_t *lock)
{
    return noza_spinlock_free(lock);
}

int nz_pthread_spin_lock(nz_pthread_spinlock_t *lock)
{
    return noza_spinlock_lock(lock);
}

int nz_pthread_spin_trylock(nz_pthread_spinlock_t *lock)
{
    return noza_spinlock_trylock(lock);
}

int nz_pthread_spin_unlock(nz_pthread_spinlock_t *lock)
{
    return noza_spinlock_unlock(lock);
}