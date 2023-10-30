#include "errno.h"
#include "sched.h"
#include "nozaos.h"
#include "noza_config.h"

int nz_sched_get_priority_max(int policy)
{
    return NOZA_OS_PRIORITY_LIMIT - 1;
}

int nz_sched_get_priority_min(int policy)
{
    return 0;
}

int nz_sched_yield(void)
{
    return noza_thread_sleep_us(0, 0);
}