#pragma once

#include <sys/types.h>

#define WNOHANG 0x01

#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) (((status) & 0x7f) != 0)
#define WTERMSIG(status) ((status) & 0x7f)

pid_t waitpid(pid_t pid, int *status, int options);
