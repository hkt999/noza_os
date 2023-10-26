#include "pthread.h"
#include "nozaos.h"

static int noza_thread_stub(void *param, uint32_t pid)
{
    nz_pthread_t *th = param;
    th->id = pid;
    return (int) th->start_routine(th->arg);
}

#include "noza_config.h"
// pthread routines
int nz_pthread_create(nz_pthread_t *thread, const nz_pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
    nz_pthread_attr_t default_attr;
    nz_pthread_attr_t *wa = (nz_pthread_attr_t *)attr;
    if (wa == NULL) {
        nz_pthread_attr_init(&default_attr);
        wa = &default_attr;
    }
    thread->start_routine = start_routine;
    thread->arg = arg;

    uint32_t priority = NOZA_OS_PRIORITY_LIMIT - 1 - wa->schedparam.sched_priority;
    uint32_t ret_code;

    if (wa->stackaddr != NULL) {
        ret_code = noza_thread_create_with_stack(&thread->id, noza_thread_stub, thread, priority, wa->stackaddr, wa->stacksize, NO_AUTO_FREE_STACK);
    } else {
        ret_code = noza_thread_create(&thread->id, noza_thread_stub, thread, priority, wa->stacksize);
    }
    if (ret_code != 0) {
        return ret_code;
    }
    if (wa->detachstate == NZ_PTHREAD_CREATE_DETACHED) {
        noza_thread_detach(thread->id);
    }
    if (attr == NULL)
        nz_pthread_attr_destroy(&default_attr);

    return 0;
}

int nz_pthread_join(nz_pthread_t thread, void **retval)
{
    uint32_t code;
    int ret;

    ret = noza_thread_join(thread.id, &code);
    if (retval != 0) {
        *retval = (void *)code;
    }
    return ret;
}

void nz_pthread_exit(void *retval)
{
    extern void noza_thread_exit(uint32_t exit_code);
    uint32_t exit_code = (uint32_t)retval;
    noza_thread_exit(exit_code);
}

int nz_pthread_detach(nz_pthread_t thread)
{
    return noza_thread_detach(thread.id);
}

int nz_pthread_yield(void)
{
    return noza_thread_sleep_us(0, NULL);
}

nz_pthread_t nz_pthread_self(void)
{
    nz_pthread_t th = {0};
    noza_thread_self(&th.id);
    return th;
}

int nz_pthread_equal(nz_pthread_t t1, nz_pthread_t t2)
{
    return t1.id == t2.id;
}

int nz_pthread_kill(nz_pthread_t thread, int sig)
{
    return noza_thread_kill(thread.id, sig);
}