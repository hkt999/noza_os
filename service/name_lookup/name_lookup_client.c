#include "name_lookup_server.h"
#include "name_lookup_client.h"
#include "nozaos.h"
#include <stdint.h>

int name_lookup_register(const char *name, uint32_t *service_id)
{
    uint32_t requested_id = service_id ? *service_id : 0;
    name_msg_t msg = {.cmd = NAME_LOOKUP_REGISTER, .name = name, .service_id = requested_id};
    noza_msg_t noza_msg = {.to_vid = NAME_SERVER_VID, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        return ret;
    }
    if (msg.code == NAME_LOOKUP_OK && service_id) {
        *service_id = msg.service_id;
    }
    return msg.code;
}

int name_lookup_resolve(const char *name, uint32_t *service_id, uint32_t *vid)
{
    name_msg_t msg = {
        .cmd = NAME_LOOKUP_RESOLVE_NAME,
        .name = name,
        .service_id = service_id ? *service_id : 0,
    };
    noza_msg_t noza_msg = {.to_vid = NAME_SERVER_VID, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        return ret;
    }
    if (msg.code == NAME_LOOKUP_OK) {
        if (service_id) {
            *service_id = msg.service_id;
        }
        if (vid) {
            *vid = msg.vid;
        }
    }
    return msg.code;
}

int name_lookup_resolve_id(uint32_t service_id, uint32_t *vid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_RESOLVE_ID, .service_id = service_id};
    noza_msg_t noza_msg = {.to_vid = NAME_SERVER_VID, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        return ret;
    }
    if (msg.code == NAME_LOOKUP_OK && vid) {
        *vid = msg.vid;
    }
    return msg.code;
}

int name_lookup_unregister(uint32_t service_id)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_UNREGISTER, .service_id = service_id};
    noza_msg_t noza_msg = {.to_vid = NAME_SERVER_VID, .ptr = (void *)&msg, .size = sizeof(msg)};
    int ret = noza_call(&noza_msg);
    if (ret != 0) {
        return ret;
    }
    return msg.code;
}
