#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "user/console/noza_console.h"

int user_root_task(void *param, uint32_t pid)
{
    console_start();
    return 0;
}
