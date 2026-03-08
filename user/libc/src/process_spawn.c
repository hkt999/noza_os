#include <string.h>
#include <sys/types.h>
#include "app_launcher.h"
#include "nozaos.h"
#include "posix/errno.h"
#include "posix/spawn.h"
#include "proc_api.h"

static int spawn_prepare_argv(const char *path, char *const argv[], char *local_argv[2], char *const **argv_out)
{
    if (path == NULL || argv_out == NULL) {
        return EINVAL;
    }
    if (argv != NULL && argv[0] != NULL) {
        *argv_out = argv;
        return 0;
    }
    local_argv[0] = (char *)path;
    local_argv[1] = NULL;
    *argv_out = local_argv;
    return 0;
}

static int spawn_resolve_path(const char *file, char resolved[NOZA_FS_MAX_PATH])
{
    if (file == NULL || resolved == NULL) {
        return EINVAL;
    }

    if (strchr(file, '/') != NULL) {
        size_t len = strnlen(file, NOZA_FS_MAX_PATH);
        if (len == 0 || len >= NOZA_FS_MAX_PATH) {
            return ENAMETOOLONG;
        }
        memcpy(resolved, file, len);
        resolved[len] = '\0';
        return 0;
    }

    static const char prefix[] = "/sbin/";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t file_len = strnlen(file, NOZA_FS_MAX_PATH);
    if (file_len == 0 || prefix_len + file_len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }
    memcpy(resolved, prefix, prefix_len);
    memcpy(resolved + prefix_len, file, file_len);
    resolved[prefix_len + file_len] = '\0';
    return 0;
}

static int spawn_call(
    pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[],
    uint32_t flags)
{
    if (path == NULL) {
        return EINVAL;
    }
    if ((file_actions != NULL && file_actions->reserved != 0) ||
        (attrp != NULL && attrp->reserved != 0)) {
        return ENOSYS;
    }

    char *local_argv[2];
    char *const *spawn_argv = NULL;
    int argv_rc = spawn_prepare_argv(path, argv, local_argv, &spawn_argv);
    if (argv_rc != 0) {
        return argv_rc;
    }

    char *const *spawn_envp = envp;
    if (spawn_envp == NULL) {
        process_record_t *process = noza_process_self();
        if (process != NULL && process->env != NULL) {
            spawn_envp = process->env->envp;
        }
    }

    uint32_t child_pid = 0;
    int rc = app_launcher_spawn(path, (char *const *)spawn_argv, (char *const *)spawn_envp, flags, &child_pid);
    if (rc == 0 && pid != NULL) {
        *pid = (pid_t)child_pid;
    }
    return rc;
}

int posix_spawn(
    pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[])
{
    return spawn_call(pid, path, file_actions, attrp, argv, envp, APP_LAUNCHER_SPAWN_FLAG_NONE);
}

int posix_spawnp(
    pid_t *pid,
    const char *file,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[])
{
    char resolved[NOZA_FS_MAX_PATH];
    int rc = spawn_resolve_path(file, resolved);
    if (rc != 0) {
        return rc;
    }
    return spawn_call(pid, resolved, file_actions, attrp, argv, envp, APP_LAUNCHER_SPAWN_FLAG_NONE);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    int rc = spawn_call(NULL, path, NULL, NULL, argv, envp, APP_LAUNCHER_SPAWN_FLAG_EXIT_SELF);
    if (rc != 0) {
        noza_set_errno(rc);
        return -1;
    }
    return 0;
}
