#include "pthread.h"

int nz_pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    return -1;
}