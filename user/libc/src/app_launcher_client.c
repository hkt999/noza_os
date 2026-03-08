#include <string.h>
#include "app_launcher.h"
#include "kernel/platform_config.h"
#include "nozaos.h"
#include "platform.h"
#include "posix/errno.h"
#include "service/memory/mem_client.h"
#include "service/name_lookup/name_lookup_client.h"

static uint32_t g_launcher_vid = 0;
static uint32_t g_launcher_service_id = 0;

static int ensure_launcher_vid(void)
{
    if (g_launcher_vid != 0) {
        return 0;
    }
    int ret = name_lookup_resolve(APP_LAUNCHER_SERVICE_NAME, &g_launcher_service_id, &g_launcher_vid);
    if (ret != NAME_LOOKUP_OK) {
        g_launcher_vid = 0;
        return ESRCH;
    }
    return 0;
}

static int copy_path(char dest[NOZA_FS_MAX_PATH], const char *src)
{
    if (src == NULL) {
        return EINVAL;
    }
    size_t len = strnlen(src, NOZA_FS_MAX_PATH);
    if (len == 0 || len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }
    memset(dest, 0, NOZA_FS_MAX_PATH);
    memcpy(dest, src, len);
    dest[len] = '\0';
    return 0;
}

static int call_launcher(app_launcher_msg_t *msg)
{
    int vid_rc = ensure_launcher_vid();
    if (vid_rc != 0) {
        return vid_rc;
    }
    noza_msg_t ipc = {.to_vid = g_launcher_vid, .ptr = (void *)msg, .size = sizeof(app_launcher_msg_t)};
    int ret = noza_call(&ipc);
    if (ret != 0) {
        g_launcher_vid = 0;
        return ret;
    }
    return msg->code;
}

static app_launcher_msg_t *alloc_launcher_msg(void)
{
    app_launcher_msg_t *msg = (app_launcher_msg_t *)noza_malloc(sizeof(app_launcher_msg_t));
    if (msg != NULL) {
        memset(msg, 0, sizeof(*msg));
    }
    return msg;
}

static uint32_t current_thread_pid(void)
{
    uint32_t pid = 0;
    if (noza_thread_self(&pid) == 0 && pid != 0) {
        return pid;
    }
    extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];
    uint32_t core = platform_get_running_core();
    if (core < NOZA_OS_NUM_CORES) {
        return NOZAOS_PID[core];
    }
    return 0;
}

int app_launcher_register(const char *path, main_t entry, uint32_t stack_size)
{
    app_launcher_msg_t *msg = alloc_launcher_msg();
    if (msg == NULL) {
        return ENOMEM;
    }
    msg->cmd = APP_LAUNCHER_REGISTER;
    msg->reg.entry = entry;
    msg->reg.stack_size = stack_size;
    msg->reg.pid = current_thread_pid();
    int cp = copy_path(msg->reg.path, path);
    if (cp != 0) {
        noza_free(msg);
        return cp;
    }
    int rc = call_launcher(msg);
    noza_free(msg);
    return rc;
}

static void pack_argv(app_launcher_msg_t *msg, char *const argv[])
{
    uint32_t argc = 0;
    if (argv) {
        while (argv[argc] && argc < APP_LAUNCHER_MAX_ARGC) {
            size_t len = strnlen(argv[argc], APP_LAUNCHER_MAX_ARG_LEN);
            if (len >= APP_LAUNCHER_MAX_ARG_LEN) {
                len = APP_LAUNCHER_MAX_ARG_LEN - 1;
            }
            memset(msg->spawn.argv[argc], 0, APP_LAUNCHER_MAX_ARG_LEN);
            memcpy(msg->spawn.argv[argc], argv[argc], len);
            msg->spawn.argv[argc][len] = '\0';
            argc++;
        }
    }
    msg->spawn.argc = argc;
}

static void pack_envp(app_launcher_msg_t *msg, char *const envp[])
{
    uint32_t envc = 0;
    if (envp) {
        while (envp[envc] && envc < APP_LAUNCHER_MAX_ENVC) {
            size_t len = strnlen(envp[envc], APP_LAUNCHER_MAX_ARG_LEN);
            if (len >= APP_LAUNCHER_MAX_ARG_LEN) {
                len = APP_LAUNCHER_MAX_ARG_LEN - 1;
            }
            memset(msg->spawn.envp[envc], 0, APP_LAUNCHER_MAX_ARG_LEN);
            memcpy(msg->spawn.envp[envc], envp[envc], len);
            msg->spawn.envp[envc][len] = '\0';
            envc++;
        }
    }
    msg->spawn.envc = envc;
}

int app_launcher_spawn(const char *path, char *const argv[], char *const envp[], uint32_t flags, uint32_t *pid_out)
{
    app_launcher_msg_t *msg = alloc_launcher_msg();
    if (msg == NULL) {
        return ENOMEM;
    }
    msg->cmd = APP_LAUNCHER_SPAWN;
    msg->spawn.flags = flags;
    msg->spawn.ppid = current_thread_pid();
    int cp = copy_path(msg->spawn.path, path);
    if (cp != 0) {
        noza_free(msg);
        return cp;
    }
    pack_argv(msg, argv);
    pack_envp(msg, envp);
    int rc = call_launcher(msg);
    if (rc != 0) {
        noza_free(msg);
        return rc;
    }
    if (pid_out) {
        *pid_out = msg->spawn.pid;
    }
    if ((flags & APP_LAUNCHER_SPAWN_FLAG_EXIT_SELF) != 0u) {
        uint32_t self_pid = msg->spawn.ppid;
        if (self_pid != 0) {
            app_launcher_exit_notify(self_pid, 0);
        }
        noza_thread_terminate(0);
    }
    noza_free(msg);
    return 0;
}

int app_launcher_list_processes(uint32_t offset, uint32_t max_items, app_launcher_msg_t *msg)
{
    if (msg == NULL) {
        return EINVAL;
    }
    app_launcher_msg_t *req = alloc_launcher_msg();
    if (req == NULL) {
        return ENOMEM;
    }
    req->cmd = APP_LAUNCHER_LIST;
    req->proc_list.offset = offset;
    req->proc_list.request_count = max_items;
    if (req->proc_list.request_count > APP_LAUNCHER_LIST_BATCH) {
        req->proc_list.request_count = APP_LAUNCHER_LIST_BATCH;
    }
    if (req->proc_list.request_count == 0) {
        req->proc_list.request_count = APP_LAUNCHER_LIST_BATCH;
    }
    int rc = call_launcher(req);
    if (rc == 0) {
        memcpy(msg, req, sizeof(*msg));
    }
    noza_free(req);
    return rc;
}

int app_launcher_list_apps(uint32_t offset, uint32_t max_items, app_launcher_msg_t *msg)
{
    if (msg == NULL) {
        return EINVAL;
    }
    app_launcher_msg_t *req = alloc_launcher_msg();
    if (req == NULL) {
        return ENOMEM;
    }
    req->cmd = APP_LAUNCHER_LIST_APPS;
    req->app_list.offset = offset;
    req->app_list.request_count = max_items;
    if (req->app_list.request_count > APP_LAUNCHER_LIST_BATCH) {
        req->app_list.request_count = APP_LAUNCHER_LIST_BATCH;
    }
    if (req->app_list.request_count == 0) {
        req->app_list.request_count = APP_LAUNCHER_LIST_BATCH;
    }
    int rc = call_launcher(req);
    if (rc == 0) {
        memcpy(msg, req, sizeof(*msg));
    }
    noza_free(req);
    return rc;
}

int app_launcher_exit_notify(uint32_t pid, int exit_code)
{
    app_launcher_msg_t *msg = alloc_launcher_msg();
    if (msg == NULL) {
        return ENOMEM;
    }
    msg->cmd = APP_LAUNCHER_EXIT_NOTIFY;
    msg->exit_notify.pid = pid;
    msg->exit_notify.exit_code = exit_code;
    if (ensure_launcher_vid() != 0) {
        noza_free(msg);
        return ESRCH;
    }
    noza_msg_t ipc = {.to_vid = g_launcher_vid, .ptr = (void *)msg, .size = sizeof(app_launcher_msg_t)};
    int ret = noza_call(&ipc);
    noza_free(msg);
    if (ret != 0) {
        g_launcher_vid = 0;
    }
    return ret;
}

int app_launcher_signal_notify(uint32_t pid, uint32_t signum)
{
    app_launcher_msg_t *msg = alloc_launcher_msg();
    if (msg == NULL) {
        return ENOMEM;
    }
    msg->cmd = APP_LAUNCHER_SIGNAL_NOTIFY;
    msg->signal_notify.pid = pid;
    msg->signal_notify.signum = signum;
    int rc = call_launcher(msg);
    noza_free(msg);
    return rc;
}

int app_launcher_wait(uint32_t ppid, int32_t pid, uint32_t options, uint32_t *reaped_pid, int32_t *status)
{
    app_launcher_msg_t *msg = alloc_launcher_msg();
    if (msg == NULL) {
        return ENOMEM;
    }
    msg->cmd = APP_LAUNCHER_WAIT;
    msg->wait.ppid = ppid;
    msg->wait.pid = pid;
    msg->wait.options = options;
    int rc = call_launcher(msg);
    if (rc == 0) {
        if (reaped_pid != NULL) {
            *reaped_pid = msg->wait.reaped_pid;
        }
        if (status != NULL) {
            *status = msg->wait.status;
        }
    }
    noza_free(msg);
    return rc;
}
