#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

_Thread_local int threadLocalVar = 0;
void root_task(void *param)
{
    console_start();
}