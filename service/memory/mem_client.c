#include "mem_serv.h"
#include "mem_client.h"
#include "nozaos.h"

#include <stdio.h>
#include <stdlib.h>

extern int32_t memory_pid; // TODO: use lookup afterwards
void *noza_malloc(size_t size)
{
    if (memory_pid < 0) {
        return malloc(size); // TODO: consider the situation before memory service is on and want to create service thread
    }
    mem_msg_t msg = {.cmd = MEMORY_MALLOC, .size = size, .ptr = NULL, .code = 0};
    noza_msg_t noza_msg = {.to_vid = memory_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        // TODO: error handling
        return NULL;
    }
    if (msg.code == MEMORY_SUCCESS) {
        return msg.ptr;
    }
    return NULL;
}

void noza_free(void *ptr)
{
    if (memory_pid < 0) {
        free(ptr);
        return;
    }
    mem_msg_t msg = {.cmd = MEMORY_FREE, .size = 0, .ptr = ptr, .code = 0};
    noza_msg_t noza_msg = {.to_vid = memory_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    return;
}

