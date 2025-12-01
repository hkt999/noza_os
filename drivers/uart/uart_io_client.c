#include <string.h>
#include "noza_console_api.h"
#include "service/name_lookup/name_lookup_client.h"
#include "nozaos.h"
#include "posix/errno.h"

static uint32_t console_service_id;
static uint32_t console_vid;

static int ensure_console_vid(uint32_t *out_vid)
{
    if (console_vid != 0) {
        if (out_vid) {
            *out_vid = console_vid;
        }
        return 0;
    }
    uint32_t vid = 0;
    int ret = name_lookup_resolve(NOZA_CONSOLE_SERVICE_NAME, &console_service_id, &vid);
    if (ret == NAME_LOOKUP_OK) {
        console_vid = vid;
        if (out_vid) {
            *out_vid = vid;
        }
        return 0;
    }
    return ret;
}

int console_write(const char *buf, uint32_t len)
{
    if (!buf || len == 0) {
        return EINVAL;
    }
    uint32_t vid = 0;
    int ret = ensure_console_vid(&vid);
    if (ret != 0) {
        return ret;
    }
    uint32_t offset = 0;
    while (offset < len) {
        console_msg_t msg = {
            .cmd = CONSOLE_CMD_WRITE,
            .len = 0,
            .code = 0
        };
        msg.len = (len - offset) > sizeof(msg.buf) ? sizeof(msg.buf) : (len - offset);
        memcpy(msg.buf, buf + offset, msg.len);
        noza_msg_t m = {.to_vid = vid, .ptr = &msg, .size = sizeof(msg)};
        ret = noza_call(&m);
        if (ret != 0) {
            console_vid = 0;
            return ret;
        }
        if (msg.code != 0) {
            return (int)msg.code;
        }
        offset += msg.len;
    }
    return 0;
}

int console_readline(char *buf, uint32_t max_len, uint32_t *out_len)
{
    if (!buf || max_len == 0) {
        return EINVAL;
    }
    uint32_t vid = 0;
    int ret = ensure_console_vid(&vid);
    if (ret != 0) {
        return ret;
    }
    console_msg_t msg = {
        .cmd = CONSOLE_CMD_READLINE,
        .len = max_len > sizeof(msg.buf) ? sizeof(msg.buf) : max_len - 1,
        .code = 0
    };
    noza_msg_t m = {.to_vid = vid, .ptr = &msg, .size = sizeof(msg)};
    ret = noza_call(&m);
    if (ret != 0) {
        console_vid = 0;
        return ret;
    }
    if (msg.code != 0) {
        return (int)msg.code;
    }
    uint32_t copy_len = msg.len < max_len - 1 ? msg.len : max_len - 1;
    memcpy(buf, msg.buf, copy_len);
    buf[copy_len] = 0;
    if (out_len) {
        *out_len = copy_len;
    }
    return 0;
}
