#include "noza_time.h"
#include "nozaos.h"

int noza_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    int64_t duration = rqtp->tv_sec * 1000000 + rqtp->tv_nsec / 1000;
    int64_t remain = 0;
    int ret = noza_thread_sleep_us(duration, &remain);
    if (rmtp) {
        rmtp->tv_sec = remain / 1000000;
        rmtp->tv_nsec = (remain % 1000000) * 1000;
    }
    if (ret == 0)
        return 0;

    // TODO: setup error here  (EINTR, EINVAL)
    return -1;
}

void noza_clock_gettime(uint32_t mode, struct timespec *ts)
{
    ts->tv_nsec = 0;
    ts->tv_nsec = 0;
}
