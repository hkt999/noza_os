#pragma once

#include <stdint.h>
#include "nozaos.h"
#include "noza_fs.h"

#define APP_LAUNCHER_SERVICE_NAME "app_launcher"
#define APP_LAUNCHER_MAX_APPS     32
#define APP_LAUNCHER_MAX_ARGC     8
#define APP_LAUNCHER_MAX_ARG_LEN  64

typedef enum {
    APP_LAUNCHER_REGISTER = 1,
    APP_LAUNCHER_LOOKUP,
    APP_LAUNCHER_SPAWN,
    APP_LAUNCHER_EXIT_NOTIFY,
} app_launcher_cmd_t;

typedef struct {
    uint32_t cmd;   // app_launcher_cmd_t
    int32_t code;   // errno style
    union {
        struct {
            char path[NOZA_FS_MAX_PATH];
            main_t entry;
            uint32_t stack_size;
        } reg;
        struct {
            char path[NOZA_FS_MAX_PATH];
            main_t entry;
            uint32_t stack_size;
        } lookup;
        struct {
            char path[NOZA_FS_MAX_PATH];
            uint32_t argc;
            char argv[APP_LAUNCHER_MAX_ARGC][APP_LAUNCHER_MAX_ARG_LEN];
            uint32_t flags;
            uint32_t pid; // optional, 0 if unknown
        } spawn;
        struct {
            uint32_t pid;
            int32_t exit_code;
        } exit_notify;
    };
} app_launcher_msg_t;

int app_launcher_register(const char *path, main_t entry, uint32_t stack_size);
int app_launcher_spawn(const char *path, char *const argv[], uint32_t flags, uint32_t *pid_out);
int app_launcher_exit_notify(uint32_t pid, int exit_code);
