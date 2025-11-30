#pragma once

#include <stdint.h>

typedef struct {
    uint32_t uid;
    uint32_t gid;
    uint32_t umask;
} noza_identity_t;

typedef struct {
    uint32_t to_vid;
    void *ptr;
    uint32_t size;
    noza_identity_t identity;
} noza_msg_t;

#define NOZA_DEFAULT_UID    0
#define NOZA_DEFAULT_GID    0
#define NOZA_DEFAULT_UMASK  0022u
