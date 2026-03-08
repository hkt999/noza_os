#include <string.h>
#include "nozaos.h"
#include "kernel/noza_config.h"
#include "posix/errno.h"
#include "app_launcher.h"
#include "spinlock.h"
#include "service/name_lookup/name_lookup_client.h"
#include "printk.h"

typedef struct {
    char path[NOZA_FS_MAX_PATH];
    main_t entry;
    uint32_t stack_size;
    uint8_t in_use;
} app_entry_t;

static app_entry_t APP_TABLE[APP_LAUNCHER_MAX_APPS];
static spinlock_t APP_LOCK;

static void lock_init_once(void)
{
    static int init = 0;
    if (!init) {
        noza_spinlock_init(&APP_LOCK);
        init = 1;
    }
}

static app_entry_t *find_app(const char *path)
{
    for (int i = 0; i < APP_LAUNCHER_MAX_APPS; i++) {
        if (APP_TABLE[i].in_use && strncmp(APP_TABLE[i].path, path, NOZA_FS_MAX_PATH) == 0) {
            return &APP_TABLE[i];
        }
    }
    return NULL;
}

static int register_app(const char *path, main_t entry, uint32_t stack_size)
{
    if (path == NULL || entry == NULL) {
        return EINVAL;
    }
    size_t len = strnlen(path, NOZA_FS_MAX_PATH);
    if (len == 0 || len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }

    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }

    app_entry_t *slot = find_app(path);
    if (slot == NULL) {
        for (int i = 0; i < APP_LAUNCHER_MAX_APPS; i++) {
            if (!APP_TABLE[i].in_use) {
                slot = &APP_TABLE[i];
                break;
            }
        }
    }
    if (slot == NULL) {
        noza_spinlock_unlock(&APP_LOCK);
        return ENOMEM;
    }

    memset(slot, 0, sizeof(*slot));
    memcpy(slot->path, path, len + 1);
    slot->entry = entry;
    slot->stack_size = stack_size;
    slot->in_use = 1;
    noza_spinlock_unlock(&APP_LOCK);
    return 0;
}

static int handle_lookup(app_launcher_msg_t *msg)
{
    app_entry_t *app = find_app(msg->lookup.path);
    if (app == NULL) {
        return ENOENT;
    }
    msg->lookup.entry = app->entry;
    msg->lookup.stack_size = app->stack_size;
    return 0;
}

static int handle_spawn(app_launcher_msg_t *msg)
{
    app_entry_t *app = find_app(msg->spawn.path);
    if (app == NULL) {
        printk("app_launcher: spawn app not found %s\n", msg->spawn.path);
        return ENOENT;
    }

    char *argv[APP_LAUNCHER_MAX_ARGC + 1] = {0};
    uint32_t argc = msg->spawn.argc;
    if (argc > APP_LAUNCHER_MAX_ARGC) {
        argc = APP_LAUNCHER_MAX_ARGC;
    }
    for (uint32_t i = 0; i < argc; i++) {
        argv[i] = msg->spawn.argv[i];
    }
    argv[argc] = NULL;

    uint32_t stack_size = app->stack_size ? app->stack_size : NOZA_THREAD_DEFAULT_STACK_SIZE;
    int rc = noza_process_exec_detached_with_stack(app->entry, (int)argc, argv, stack_size);
    if (rc != 0) {
        printk("app_launcher: spawn failed rc=%d\n", rc);
        return rc;
    }
    msg->spawn.pid = 0; // pid unknown with current API
    return 0;
}

static void dispatch(app_launcher_msg_t *msg)
{
    msg->code = 0;
    switch (msg->cmd) {
    case APP_LAUNCHER_REGISTER:
        msg->code = register_app(msg->reg.path, msg->reg.entry, msg->reg.stack_size);
        break;
    case APP_LAUNCHER_LOOKUP:
        msg->code = handle_lookup(msg);
        break;
    case APP_LAUNCHER_SPAWN:
        msg->code = handle_spawn(msg);
        break;
    case APP_LAUNCHER_EXIT_NOTIFY:
        msg->code = 0; // reserved for future tracking
        break;
    default:
        msg->code = ENOSYS;
        break;
    }
}

static int app_launcher_service(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    lock_init_once();
    memset(APP_TABLE, 0, sizeof(APP_TABLE));

    uint32_t service_id = 0;
    int reg_ret = name_lookup_register(APP_LAUNCHER_SERVICE_NAME, &service_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printk("app_launcher: name register failed (%d)\n", reg_ret);
    } else {
        printk("app_launcher: registered vid=%u\n", service_id);
    }

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) != 0) {
            continue;
        }
        if (msg.ptr && msg.size >= sizeof(app_launcher_msg_t)) {
            app_launcher_msg_t *req = (app_launcher_msg_t *)msg.ptr;
            dispatch(req);
        }
        noza_reply(&msg);
    }
    return 0;
}

static uint8_t app_launcher_stack[1024];
void __attribute__((constructor(105))) app_launcher_init(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(app_launcher_service, app_launcher_stack, sizeof(app_launcher_stack));
}
