#include <string.h>
#include "app_launcher.h"
#include "nozaos.h"
#include "posix/errno.h"
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

int app_launcher_register(const char *path, main_t entry, uint32_t stack_size)
{
    app_launcher_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = APP_LAUNCHER_REGISTER;
    msg.reg.entry = entry;
    msg.reg.stack_size = stack_size;
    int cp = copy_path(msg.reg.path, path);
    if (cp != 0) {
        return cp;
    }
    return call_launcher(&msg);
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

int app_launcher_spawn(const char *path, char *const argv[], uint32_t flags, uint32_t *pid_out)
{
    (void)pid_out; // pid not available in current implementation
    app_launcher_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = APP_LAUNCHER_SPAWN;
    msg.spawn.flags = flags;
    int cp = copy_path(msg.spawn.path, path);
    if (cp != 0) {
        return cp;
    }
    pack_argv(&msg, argv);
    return call_launcher(&msg);
}

int app_launcher_exit_notify(uint32_t pid, int exit_code)
{
    app_launcher_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = APP_LAUNCHER_EXIT_NOTIFY;
    msg.exit_notify.pid = pid;
    msg.exit_notify.exit_code = exit_code;
    if (ensure_launcher_vid() != 0) {
        return ESRCH;
    }
    noza_msg_t ipc = {.to_vid = g_launcher_vid, .ptr = (void *)&msg, .size = sizeof(app_launcher_msg_t)};
    int ret = noza_nonblock_call(&ipc);
    if (ret != 0) {
        g_launcher_vid = 0;
    }
    return ret;
}
