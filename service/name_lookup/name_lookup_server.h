#pragma once

#define NAME_LOOKUP_REGISTER    1
#define NAME_LOOKUP_LOOKUP      2
#define NAME_LOOKUP_UNREGISTER  3

typedef struct name_msg_s {
    uint32_t cmd;
    const char *name;
    uint32_t value;
    uint32_t code;
} name_msg_t;
