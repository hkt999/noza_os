#pragma once
#include <stdint.h>

typedef int (*main_func_t)(int argc, char **argv);
typedef struct {
    const char *name;
    main_func_t main_func;
    const char *help_msg;
} builtin_cmd_t;

int console_start(void *param, uint32_t pid);
int console_stop();
void console_add_command(const char *name, int (*main)(int argc, char **argv), const char *desc);
