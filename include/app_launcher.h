#pragma once

#include <stdint.h>
#include "nozaos.h"
#include "noza_fs.h"

#define APP_LAUNCHER_SERVICE_NAME "app_launcher"
#define APP_LAUNCHER_MAX_APPS     32
#define APP_LAUNCHER_MAX_PROCS    32
#define APP_LAUNCHER_MAX_ARGC     8
#define APP_LAUNCHER_MAX_ENVC     8
#define APP_LAUNCHER_MAX_ARG_LEN  64
#define APP_LAUNCHER_LIST_BATCH   8

#define APP_LAUNCHER_SPAWN_FLAG_NONE       0u
#define APP_LAUNCHER_SPAWN_FLAG_EXIT_SELF  (1u << 0)

typedef enum {
    APP_LAUNCHER_REGISTER = 1,
    APP_LAUNCHER_LOOKUP,
    APP_LAUNCHER_SPAWN,
    APP_LAUNCHER_LIST,
    APP_LAUNCHER_LIST_APPS,
    APP_LAUNCHER_EXIT_NOTIFY,
    APP_LAUNCHER_SIGNAL_NOTIFY,
    APP_LAUNCHER_WAIT,
} app_launcher_cmd_t;

typedef struct {
    char path[NOZA_FS_MAX_PATH];
    uint32_t stack_size;
} app_launcher_app_item_t;

typedef enum {
    APP_LAUNCHER_PROC_STATE_RUNNING = 1,
    APP_LAUNCHER_PROC_STATE_STOPPED = 2,
    APP_LAUNCHER_PROC_STATE_EXITED = 3,
} app_launcher_proc_state_t;

typedef struct {
    uint32_t pid;
    uint32_t ppid;
    uint32_t thread_count;
    uint32_t state;
    int32_t exit_code;
    char path[NOZA_FS_MAX_PATH];
} app_launcher_proc_item_t;

typedef struct {
    uint32_t cmd;   // app_launcher_cmd_t
    int32_t code;   // errno style
    union {
        struct {
            char path[NOZA_FS_MAX_PATH];
            main_t entry;
            uint32_t stack_size;
            uint32_t pid;
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
            uint32_t envc;
            char envp[APP_LAUNCHER_MAX_ENVC][APP_LAUNCHER_MAX_ARG_LEN];
            uint32_t flags;
            uint32_t pid; // optional, 0 if unknown
            uint32_t ppid;
        } spawn;
        struct {
            uint32_t offset;
            uint32_t request_count;
            uint32_t count;
            uint32_t total;
            app_launcher_proc_item_t items[APP_LAUNCHER_LIST_BATCH];
        } proc_list;
        struct {
            uint32_t offset;
            uint32_t request_count;
            uint32_t count;
            uint32_t total;
            app_launcher_app_item_t items[APP_LAUNCHER_LIST_BATCH];
        } app_list;
        struct {
            uint32_t pid;
            int32_t exit_code;
        } exit_notify;
        struct {
            uint32_t pid;
            uint32_t signum;
        } signal_notify;
        struct {
            int32_t pid;
            uint32_t ppid;
            uint32_t options;
            uint32_t reaped_pid;
            int32_t status;
        } wait;
    };
} app_launcher_msg_t;

int app_launcher_register(const char *path, main_t entry, uint32_t stack_size);
int app_launcher_spawn(const char *path, char *const argv[], char *const envp[], uint32_t flags, uint32_t *pid_out);
int app_launcher_list_processes(uint32_t offset, uint32_t max_items, app_launcher_msg_t *msg);
int app_launcher_list_apps(uint32_t offset, uint32_t max_items, app_launcher_msg_t *msg);
int app_launcher_exit_notify(uint32_t pid, int exit_code);
int app_launcher_signal_notify(uint32_t pid, uint32_t signum);
int app_launcher_wait(uint32_t ppid, int32_t pid, uint32_t options, uint32_t *reaped_pid, int32_t *status);
