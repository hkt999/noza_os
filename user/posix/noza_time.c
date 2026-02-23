#include "noza_time.h"
#include "nozaos.h"

int nz_nanosleep(const struct nz_timespec *rqtp, struct nz_timespec *rmtp)
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

void nz_clock_gettime(uint32_t mode, struct nz_timespec *ts)
{
    if (ts == NULL) {
        return;
    }
    noza_time64_t noza_ts = {0};
    if (noza_clock_gettime(mode, &noza_ts) != 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
        return;
    }
    uint64_t us = ((uint64_t)noza_ts.high << 32) | noza_ts.low;
    ts->tv_sec = (uint32_t)(us / 1000000ULL);
    ts->tv_nsec = (uint32_t)((us % 1000000ULL) * 1000ULL);
}

int nz_sleep(unsigned int seconds)
 {
    return noza_thread_sleep_us(seconds * 1000000, 0);
 }
 
 int nz_usleep(unsigned int usec)
 {
    return noza_thread_sleep_us(usec, 0);
 }
