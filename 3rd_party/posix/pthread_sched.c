#include "errno.h"
#include "sched.h"
#include "nozaos.h"
#include "noza_config.h"

int sched_get_priority_max(int policy)
{
    return NOZA_OS_PRIORITY_LIMIT - 1;
}

int sched_get_priority_min(int policy)
{
    return 0;
}

int sched_yield(void)
{
    return noza_thread_yield();
}