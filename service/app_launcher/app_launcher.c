#include <string.h>
#include "nozaos.h"
#include "kernel/noza_config.h"
#include "posix/errno.h"
#include "posix/bits/signum.h"
#include <sys/wait.h>
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

typedef struct {
    uint32_t pid;
    uint32_t ppid;
    uint32_t thread_count;
    uint32_t state;
    uint32_t term_signal;
    int32_t exit_code;
    char path[NOZA_FS_MAX_PATH];
    uint8_t in_use;
} proc_entry_t;

typedef struct wait_pending_s {
    struct wait_pending_s *next;
    noza_msg_t noza_msg;
    uint32_t ppid;
    int32_t pid;
    uint32_t options;
    uint8_t in_use;
} wait_pending_t;

static app_entry_t APP_TABLE[APP_LAUNCHER_MAX_APPS];
static proc_entry_t PROC_TABLE[APP_LAUNCHER_MAX_PROCS];
static wait_pending_t WAIT_PENDING_POOL[APP_LAUNCHER_MAX_PROCS];
static wait_pending_t *WAIT_PENDING_HEAD;
static wait_pending_t *WAIT_PENDING_FREE;
static spinlock_t APP_LOCK;

static void lock_init_once(void)
{
    static int init = 0;
    if (!init) {
        noza_spinlock_init(&APP_LOCK);
        WAIT_PENDING_HEAD = NULL;
        WAIT_PENDING_FREE = &WAIT_PENDING_POOL[0];
        for (int i = 0; i < APP_LAUNCHER_MAX_PROCS - 1; i++) {
            WAIT_PENDING_POOL[i].next = &WAIT_PENDING_POOL[i + 1];
        }
        WAIT_PENDING_POOL[APP_LAUNCHER_MAX_PROCS - 1].next = NULL;
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

static proc_entry_t *find_proc(uint32_t pid)
{
    if (pid == 0) {
        return NULL;
    }
    for (int i = 0; i < APP_LAUNCHER_MAX_PROCS; i++) {
        if (PROC_TABLE[i].in_use && PROC_TABLE[i].pid == pid) {
            return &PROC_TABLE[i];
        }
    }
    return NULL;
}

static proc_entry_t *alloc_proc_slot(void)
{
    for (int i = 0; i < APP_LAUNCHER_MAX_PROCS; i++) {
        if (!PROC_TABLE[i].in_use) {
            return &PROC_TABLE[i];
        }
    }
    for (int i = 0; i < APP_LAUNCHER_MAX_PROCS; i++) {
        if (PROC_TABLE[i].state != APP_LAUNCHER_PROC_STATE_RUNNING) {
            return &PROC_TABLE[i];
        }
    }
    return NULL;
}

static int record_process(uint32_t pid, uint32_t ppid, const char *path, uint32_t state,
    int32_t exit_code, uint32_t term_signal)
{
    if (pid == 0) {
        return 0;
    }

    proc_entry_t *slot = find_proc(pid);
    if (slot == NULL) {
        slot = alloc_proc_slot();
        if (slot == NULL) {
            return ENOMEM;
        }
        memset(slot, 0, sizeof(*slot));
    }

    slot->in_use = 1;
    slot->pid = pid;
    if (ppid != 0) {
        slot->ppid = ppid;
    }
    slot->state = state;
    slot->exit_code = exit_code;
    slot->term_signal = term_signal;
    if (state == APP_LAUNCHER_PROC_STATE_EXITED) {
        slot->thread_count = 0;
    }
    if (path != NULL && path[0] != '\0') {
        strncpy(slot->path, path, NOZA_FS_MAX_PATH - 1);
        slot->path[NOZA_FS_MAX_PATH - 1] = '\0';
    }
    return 0;
}

static void clear_process(proc_entry_t *slot)
{
    if (slot != NULL) {
        memset(slot, 0, sizeof(*slot));
    }
}

static void refresh_runtime_info(proc_entry_t *slot)
{
    if (slot == NULL || !slot->in_use || slot->state != APP_LAUNCHER_PROC_STATE_RUNNING) {
        return;
    }

    noza_process_info_t info;
    if (noza_process_get_info(slot->pid, &info) != 0) {
        return;
    }
    if (info.parent_pid != 0) {
        slot->ppid = info.parent_pid;
    }
    slot->thread_count = info.thread_count;
}

static int32_t encode_wait_status(const proc_entry_t *slot)
{
    if (slot == NULL) {
        return 0;
    }
    if (slot->term_signal != 0) {
        return (int32_t)(slot->term_signal & 0x7f);
    }
    return (slot->exit_code & 0xff) << 8;
}

static wait_pending_t *alloc_wait_pending(void)
{
    wait_pending_t *node = WAIT_PENDING_FREE;
    if (node == NULL) {
        return NULL;
    }
    WAIT_PENDING_FREE = node->next;
    memset(node, 0, sizeof(*node));
    node->in_use = 1;
    return node;
}

static void free_wait_pending(wait_pending_t *node)
{
    if (node == NULL) {
        return;
    }
    memset(node, 0, sizeof(*node));
    node->next = WAIT_PENDING_FREE;
    WAIT_PENDING_FREE = node;
}

static proc_entry_t *find_wait_target_locked(uint32_t ppid, int32_t pid, int *have_child)
{
    proc_entry_t *selected = NULL;

    if (have_child != NULL) {
        *have_child = 0;
    }
    for (int i = 0; i < APP_LAUNCHER_MAX_PROCS; i++) {
        proc_entry_t *slot = &PROC_TABLE[i];
        if (!slot->in_use || slot->ppid != ppid) {
            continue;
        }
        if (have_child != NULL) {
            *have_child = 1;
        }
        refresh_runtime_info(slot);
        if (pid > 0 && slot->pid != (uint32_t)pid) {
            continue;
        }
        if (slot->state == APP_LAUNCHER_PROC_STATE_EXITED) {
            return slot;
        }
        if (selected == NULL && ((pid > 0 && slot->pid == (uint32_t)pid) || pid == -1)) {
            selected = slot;
        }
    }
    return selected;
}

static void service_pending_waits_locked(void)
{
    wait_pending_t *prev = NULL;
    wait_pending_t *node = WAIT_PENDING_HEAD;

    while (node != NULL) {
        app_launcher_msg_t *msg = (app_launcher_msg_t *)node->noza_msg.ptr;
        int have_child = 0;
        proc_entry_t *selected = find_wait_target_locked(node->ppid, node->pid, &have_child);
        int should_reply = 0;

        if (!have_child || selected == NULL) {
            msg->code = ECHILD;
            should_reply = 1;
        } else if (selected->state == APP_LAUNCHER_PROC_STATE_EXITED) {
            msg->wait.reaped_pid = selected->pid;
            msg->wait.status = encode_wait_status(selected);
            msg->code = 0;
            clear_process(selected);
            should_reply = 1;
        }

        if (!should_reply) {
            prev = node;
            node = node->next;
            continue;
        }

        wait_pending_t *done = node;
        node = node->next;
        if (prev == NULL) {
            WAIT_PENDING_HEAD = node;
        } else {
            prev->next = node;
        }
        noza_reply(&done->noza_msg);
        free_wait_pending(done);
    }
}

static int load_app(const char *path, app_entry_t *out)
{
    if (path == NULL || out == NULL) {
        return EINVAL;
    }
    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }
    app_entry_t *app = find_app(path);
    if (app == NULL) {
        noza_spinlock_unlock(&APP_LOCK);
        return ENOENT;
    }
    *out = *app;
    noza_spinlock_unlock(&APP_LOCK);
    return 0;
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
    app_entry_t app;
    int rc = load_app(msg->lookup.path, &app);
    if (rc != 0) {
        return rc;
    }
    msg->lookup.entry = app.entry;
    msg->lookup.stack_size = app.stack_size;
    return 0;
}

static int handle_spawn(app_launcher_msg_t *msg)
{
    app_entry_t app;
    int lookup_rc = load_app(msg->spawn.path, &app);
    if (lookup_rc != 0) {
        printk("app_launcher: spawn app not found %s\n", msg->spawn.path);
        return lookup_rc;
    }

    char *argv[APP_LAUNCHER_MAX_ARGC + 1] = {0};
    char *envp[APP_LAUNCHER_MAX_ENVC + 1] = {0};
    uint32_t argc = msg->spawn.argc;
    uint32_t envc = msg->spawn.envc;
    if (argc > APP_LAUNCHER_MAX_ARGC) {
        argc = APP_LAUNCHER_MAX_ARGC;
    }
    if (envc > APP_LAUNCHER_MAX_ENVC) {
        envc = APP_LAUNCHER_MAX_ENVC;
    }
    for (uint32_t i = 0; i < argc; i++) {
        argv[i] = msg->spawn.argv[i];
    }
    argv[argc] = NULL;
    for (uint32_t i = 0; i < envc; i++) {
        envp[i] = msg->spawn.envp[i];
    }
    envp[envc] = NULL;

    uint32_t stack_size = app.stack_size ? app.stack_size : NOZA_THREAD_DEFAULT_STACK_SIZE;
    uint32_t pid = 0;
    int rc = noza_process_spawn_detached_ex(app.entry, (int)argc, argv, envp, msg->spawn.ppid, stack_size, &pid);
    if (rc != 0) {
        printk("app_launcher: spawn failed rc=%d\n", rc);
        return rc;
    }
    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }
    rc = record_process(pid, msg->spawn.ppid, app.path, APP_LAUNCHER_PROC_STATE_RUNNING, 0, 0);
    noza_spinlock_unlock(&APP_LOCK);
    if (rc != 0) {
        return rc;
    }
    msg->spawn.pid = pid;
    return 0;
}

static int handle_list_processes(app_launcher_msg_t *msg)
{
    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }

    uint32_t offset = msg->proc_list.offset;
    uint32_t request_count = msg->proc_list.request_count;
    if (request_count == 0 || request_count > APP_LAUNCHER_LIST_BATCH) {
        request_count = APP_LAUNCHER_LIST_BATCH;
    }

    uint32_t total = 0;
    uint32_t emitted = 0;
    memset(msg->proc_list.items, 0, sizeof(msg->proc_list.items));
    for (int i = 0; i < APP_LAUNCHER_MAX_PROCS; i++) {
        if (!PROC_TABLE[i].in_use) {
            continue;
        }
        refresh_runtime_info(&PROC_TABLE[i]);
        if (total >= offset && emitted < request_count) {
            msg->proc_list.items[emitted].pid = PROC_TABLE[i].pid;
            msg->proc_list.items[emitted].ppid = PROC_TABLE[i].ppid;
            msg->proc_list.items[emitted].thread_count = PROC_TABLE[i].thread_count;
            msg->proc_list.items[emitted].state = PROC_TABLE[i].state;
            msg->proc_list.items[emitted].exit_code = PROC_TABLE[i].exit_code;
            strncpy(msg->proc_list.items[emitted].path, PROC_TABLE[i].path, NOZA_FS_MAX_PATH - 1);
            msg->proc_list.items[emitted].path[NOZA_FS_MAX_PATH - 1] = '\0';
            emitted++;
        }
        total++;
    }
    msg->proc_list.request_count = request_count;
    msg->proc_list.count = emitted;
    msg->proc_list.total = total;
    noza_spinlock_unlock(&APP_LOCK);
    return 0;
}

static int handle_list_apps(app_launcher_msg_t *msg)
{
    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }

    uint32_t offset = msg->app_list.offset;
    uint32_t request_count = msg->app_list.request_count;
    if (request_count == 0 || request_count > APP_LAUNCHER_LIST_BATCH) {
        request_count = APP_LAUNCHER_LIST_BATCH;
    }

    uint32_t total = 0;
    uint32_t emitted = 0;
    memset(msg->app_list.items, 0, sizeof(msg->app_list.items));
    for (int i = 0; i < APP_LAUNCHER_MAX_APPS; i++) {
        if (!APP_TABLE[i].in_use) {
            continue;
        }
        if (total >= offset && emitted < request_count) {
            strncpy(msg->app_list.items[emitted].path, APP_TABLE[i].path, NOZA_FS_MAX_PATH - 1);
            msg->app_list.items[emitted].path[NOZA_FS_MAX_PATH - 1] = '\0';
            msg->app_list.items[emitted].stack_size = APP_TABLE[i].stack_size;
            emitted++;
        }
        total++;
    }
    msg->app_list.request_count = request_count;
    msg->app_list.count = emitted;
    msg->app_list.total = total;
    noza_spinlock_unlock(&APP_LOCK);
    return 0;
}

static int handle_exit_notify(app_launcher_msg_t *msg)
{
    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }
    int rc = record_process(
        msg->exit_notify.pid, 0, NULL, APP_LAUNCHER_PROC_STATE_EXITED, msg->exit_notify.exit_code, 0);
    service_pending_waits_locked();
    noza_spinlock_unlock(&APP_LOCK);
    return rc;
}

static int handle_signal_notify(app_launcher_msg_t *msg)
{
    uint32_t state = APP_LAUNCHER_PROC_STATE_RUNNING;
    int32_t exit_code = 0;

    switch (msg->signal_notify.signum) {
    case SIGSTOP:
    case SIGTSTP:
        state = APP_LAUNCHER_PROC_STATE_STOPPED;
        break;
    case SIGCONT:
        state = APP_LAUNCHER_PROC_STATE_RUNNING;
        break;
    case SIGTERM:
    case SIGKILL:
        state = APP_LAUNCHER_PROC_STATE_EXITED;
        exit_code = 128 + (int32_t)msg->signal_notify.signum;
        break;
    default:
        return 0;
    }

    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        return EBUSY;
    }
    int rc = record_process(
        msg->signal_notify.pid, 0, NULL, state, exit_code,
        state == APP_LAUNCHER_PROC_STATE_EXITED ? msg->signal_notify.signum : 0);
    if (state == APP_LAUNCHER_PROC_STATE_EXITED) {
        service_pending_waits_locked();
    }
    noza_spinlock_unlock(&APP_LOCK);
    return rc;
}

static int handle_wait(noza_msg_t *ipc_msg)
{
    app_launcher_msg_t *msg = (app_launcher_msg_t *)ipc_msg->ptr;
    int have_child = 0;

    lock_init_once();
    if (noza_spinlock_lock(&APP_LOCK) != 0) {
        msg->code = EBUSY;
        return 1;
    }

    proc_entry_t *selected = find_wait_target_locked(msg->wait.ppid, msg->wait.pid, &have_child);
    if (!have_child || selected == NULL) {
        msg->code = ECHILD;
        noza_spinlock_unlock(&APP_LOCK);
        return 1;
    }

    if (selected->state != APP_LAUNCHER_PROC_STATE_EXITED) {
        if ((msg->wait.options & WNOHANG) != 0u) {
            msg->wait.reaped_pid = 0;
            msg->wait.status = 0;
            msg->code = 0;
            noza_spinlock_unlock(&APP_LOCK);
            return 1;
        }
        wait_pending_t *pending = alloc_wait_pending();
        if (pending == NULL) {
            msg->code = ENOMEM;
            noza_spinlock_unlock(&APP_LOCK);
            return 1;
        }
        pending->noza_msg = *ipc_msg;
        pending->ppid = msg->wait.ppid;
        pending->pid = msg->wait.pid;
        pending->options = msg->wait.options;
        pending->next = WAIT_PENDING_HEAD;
        WAIT_PENDING_HEAD = pending;
        noza_spinlock_unlock(&APP_LOCK);
        return 0;
    }

    msg->wait.reaped_pid = selected->pid;
    msg->wait.status = encode_wait_status(selected);
    clear_process(selected);
    msg->code = 0;
    service_pending_waits_locked();
    noza_spinlock_unlock(&APP_LOCK);
    return 1;
}

static int dispatch(noza_msg_t *ipc_msg)
{
    app_launcher_msg_t *msg = (app_launcher_msg_t *)ipc_msg->ptr;
    msg->code = 0;
    switch (msg->cmd) {
    case APP_LAUNCHER_REGISTER:
        msg->code = register_app(msg->reg.path, msg->reg.entry, msg->reg.stack_size);
        if (msg->code == 0 && msg->reg.pid != 0) {
            lock_init_once();
            if (noza_spinlock_lock(&APP_LOCK) == 0) {
                int rc = 0;
                if (find_proc(msg->reg.pid) == NULL) {
                    rc = record_process(msg->reg.pid, 0, msg->reg.path, APP_LAUNCHER_PROC_STATE_RUNNING, 0, 0);
                }
                noza_spinlock_unlock(&APP_LOCK);
                if (rc != 0) {
                    msg->code = rc;
                }
            } else {
                msg->code = EBUSY;
            }
        }
        return 1;
    case APP_LAUNCHER_LOOKUP:
        msg->code = handle_lookup(msg);
        return 1;
    case APP_LAUNCHER_SPAWN:
        msg->code = handle_spawn(msg);
        return 1;
    case APP_LAUNCHER_LIST:
        msg->code = handle_list_processes(msg);
        return 1;
    case APP_LAUNCHER_LIST_APPS:
        msg->code = handle_list_apps(msg);
        return 1;
    case APP_LAUNCHER_EXIT_NOTIFY:
        msg->code = handle_exit_notify(msg);
        return 1;
    case APP_LAUNCHER_SIGNAL_NOTIFY:
        msg->code = handle_signal_notify(msg);
        return 1;
    case APP_LAUNCHER_WAIT:
        return handle_wait(ipc_msg);
    default:
        msg->code = ENOSYS;
        return 1;
    }
}

static int app_launcher_service(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    lock_init_once();
    memset(APP_TABLE, 0, sizeof(APP_TABLE));
    memset(PROC_TABLE, 0, sizeof(PROC_TABLE));

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
            if (dispatch(&msg)) {
                noza_reply(&msg);
            }
        } else {
            noza_reply(&msg);
        }
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
