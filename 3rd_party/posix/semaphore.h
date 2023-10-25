#pragma once

#include "pthread.h"

typedef struct {
    nz_pthread_mutex_t mutex;
    nz_pthread_cond_t cond;
    int count;
} nz_sem_t;

int nz_sem_init(nz_sem_t *sem, int pshared, unsigned int value);
int nz_sem_destroy(nz_sem_t *sem);
int nz_sem_wait(nz_sem_t *sem);
int nz_sem_trywait(nz_sem_t *sem);
int nz_sem_post(nz_sem_t *sem);
int nz_sem_getvalue(nz_sem_t *sem, int *sval);
