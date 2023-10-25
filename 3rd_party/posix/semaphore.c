#include "semaphore.h"

int nz_sem_init(nz_sem_t *sem, int pshared, unsigned int value)
{
    if (pshared) {
        // this simplified implementation does not support shared semaphores
        return -1;
    }

    sem->count = value;
    nz_pthread_mutex_init(&sem->mutex, NULL);
    nz_pthread_cond_init(&sem->cond, NULL);
    return 0;
}

int nz_sem_destroy(nz_sem_t *sem)
{
    nz_pthread_mutex_destroy(&sem->mutex);
    nz_pthread_cond_destroy(&sem->cond);
    return 0;
}

int nz_sem_wait(nz_sem_t *sem)
{
    nz_pthread_mutex_lock(&sem->mutex);
    while (sem->count <= 0) {
        nz_pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    nz_pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int nz_sem_post(nz_sem_t *sem)
{
    nz_pthread_mutex_lock(&sem->mutex);
    sem->count++;
    nz_pthread_cond_signal(&sem->cond); // signal one waiting thread
    nz_pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int nz_sem_trywait(nz_sem_t *sem)
{
    int ret = 0;
    nz_pthread_mutex_lock(&sem->mutex);
    if (sem->count > 0) {
        sem->count--;
    } else {
        ret = EAGAIN; // try again error, as per POSIX
    }
    nz_pthread_mutex_unlock(&sem->mutex);
    return ret;
}

int sem_getvalue(nz_sem_t *sem, int *sval)
{
    nz_pthread_mutex_lock(&sem->mutex);
    *sval = sem->count;
    nz_pthread_mutex_unlock(&sem->mutex);
    return 0;
}

