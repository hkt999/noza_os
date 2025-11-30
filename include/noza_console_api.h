#pragma once

#include <stdint.h>

#define NOZA_CONSOLE_SERVICE_NAME "console.io"

typedef enum {
    CONSOLE_CMD_READLINE = 1,
    CONSOLE_CMD_WRITE = 2
} console_cmd_t;

typedef struct {
    uint32_t cmd;
    uint32_t len;     // in/out for write/readline
    char     buf[128];
    uint32_t code;    // status (0 ok)
} console_msg_t;
