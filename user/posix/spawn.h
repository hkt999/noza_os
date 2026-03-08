#pragma once

#include <sys/types.h>

typedef struct {
    int reserved;
} posix_spawn_file_actions_t;

typedef struct {
    int reserved;
} posix_spawnattr_t;

int posix_spawn(
    pid_t *pid,
    const char *path,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]);

int posix_spawnp(
    pid_t *pid,
    const char *file,
    const posix_spawn_file_actions_t *file_actions,
    const posix_spawnattr_t *attrp,
    char *const argv[],
    char *const envp[]);

int execve(const char *path, char *const argv[], char *const envp[]);
