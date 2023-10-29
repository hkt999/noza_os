#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

void root_task(void *param)
{
    #if 0
    uint32_t th;
    noza_thread_create(&th, console_start, NULL, 0, 4096);
    noza_thread_join(th, NULL);
    #else
    console_start();
    //console_start(&table.builtin_cmds[0], 0);
    #endif
}