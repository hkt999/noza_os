#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

#ifdef NOZAOS_TFLM
extern void RegisterDebugLogCallback(void (*cb)(const char* s));
void tflm_debug(const char *s)
{
    printf("%s\n", s);
}
#endif

void root_task(void *param)
{
    #if 1
    uint32_t th;
    noza_thread_create(&th, console_start, NULL, 0, 2048);
    noza_thread_join(th, NULL);
    #else
    console_start(&table.builtin_cmds[0], 0);
    #endif
}