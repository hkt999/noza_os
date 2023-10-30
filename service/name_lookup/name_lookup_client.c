#include "name_lookup_server.h"
#include "name_lookup_client.h"
#include "nozaos.h"
#include <stdint.h>

int name_lookup_register(const char *name, uint32_t value)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_REGISTER, .name = name, .value = value};
    noza_msg_t noza_msg = {.to_pid = NAME_SERVER_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);

    return msg.code;
}

int name_lookup_search(const char *name, uint32_t *value)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_SEARCH, .name = name};
    noza_msg_t noza_msg = {.to_pid = NAME_SERVER_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    if (msg.code == 0) {
        *value = msg.value;
    }

    return msg.code;
}

int name_lookup_unregister(const char *name)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_UNREGISTER, .name = name};
    noza_msg_t noza_msg = {.to_pid = NAME_SERVER_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);

    return msg.code;
}
