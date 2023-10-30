#include "semaphore.h"

int nz_sem_init(nz_sem_t *sem, int pshared, unsigned int value)
{
    if (pshared) {
        // this simplified implementation does not support shared semaphores
        return -1;
    }
    semaphore_init(sem, value);

    return 0;
}

int nz_sem_destroy(nz_sem_t *sem)
{
    return semaphore_destroy(sem);
}

int nz_sem_wait(nz_sem_t *sem)
{
    return semaphore_wait(sem);
}

int nz_sem_post(nz_sem_t *sem)
{
    return semaphore_post(sem);
}

int nz_sem_trywait(nz_sem_t *sem)
{
    return semaphore_trywait(sem);
}

int nz_sem_getvalue(nz_sem_t *sem, int *sval)
{
    return semaphore_getvalue(sem, sval);
}

