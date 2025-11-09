#include "mem_serv.h"
#include "mem_client.h"
#include "nozaos.h"
#include "../name_lookup/name_lookup_client.h"

#include <stdlib.h>

static uint32_t memory_service_id;
static uint32_t memory_vid;

static int ensure_memory_vid(uint32_t *resolved_vid)
{
    if (memory_vid != 0) {
        if (resolved_vid) {
            *resolved_vid = memory_vid;
        }
        return 0;
    }

    uint32_t vid = 0;
    int ret = name_lookup_resolve(NOZA_MEMORY_SERVICE_NAME, &memory_service_id, &vid);
    if (ret == NAME_LOOKUP_OK) {
        memory_vid = vid;
        if (resolved_vid) {
            *resolved_vid = vid;
        }
        return 0;
    }

    return ret;
}

void *noza_malloc(size_t size)
{
    uint32_t target_vid = 0;
    if (ensure_memory_vid(&target_vid) != 0) {
        return malloc(size);
    }
    mem_msg_t msg = {.cmd = MEMORY_MALLOC, .size = size, .ptr = NULL, .code = 0};
    noza_msg_t noza_msg = {.to_vid = target_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        memory_vid = 0;
        return malloc(size);
    }
    if (msg.code == MEMORY_SUCCESS) {
        return msg.ptr;
    }
    return NULL;
}

void noza_free(void *ptr)
{
    uint32_t target_vid = 0;
    if (ensure_memory_vid(&target_vid) != 0) {
        free(ptr);
        return;
    }
    mem_msg_t msg = {.cmd = MEMORY_FREE, .size = 0, .ptr = ptr, .code = 0};
    noza_msg_t noza_msg = {.to_vid = target_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
    if (noza_call(&noza_msg) != 0) {
        memory_vid = 0;
        free(ptr);
    }
    return;
}
