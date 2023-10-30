#pragma once

#include <stdint.h>

struct nz_timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

typedef struct {
    uint32_t id;
} nz_clockid_t;

int nz_nanosleep(const struct nz_timespec *rqtp, struct nz_timespec *rmtp);

#define SZ_CLOCK_REALTIME 0
void nz_clock_gettime(uint32_t mode, struct nz_timespec *ts);
int nz_sleep(unsigned int seconds);
int nz_usleep(unsigned int usec);