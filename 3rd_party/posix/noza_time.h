#pragma once

#include <stdint.h>

struct noza_timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

typedef struct {
    uint32_t id;
} noza_clockid_t;

int noza_nanosleep(const struct noza_timespec *rqtp, struct noza_timespec *rmtp);

#define CLOCK_REALTIME 0
void noza_clock_gettime(uint32_t mode, struct noza_timespec *ts);

#define timespec noza_timespec // overwrite
#define clockid_t noza_clockid_t  // overwrite