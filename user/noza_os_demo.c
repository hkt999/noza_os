#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"
#include "user/libc/src/proc_api.h"
#include "apps/shell/shell_main.h"

int user_root_task(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    noza_process_exec_detached_with_stack(shell_main, 0, NULL, 2048);
    return 0;
}
