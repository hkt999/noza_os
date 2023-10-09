#pragma once

#include <stdint.h>

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

typedef struct {
} clockid_t;

int noza_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#define CLOCK_REALTIME 0
void noza_clock_gettime(uint32_t mode, struct timespec *ts);