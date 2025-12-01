#include <string.h>
#include "nozaos.h"
#include "posix/errno.h"
#include "noza_fs.h"
#include "service/name_lookup/name_lookup_client.h"
#include "fs_dispatch.h"
#include "vfs.h"
#include "ramfs.h"
#include "devfs.h"
#include "drivers/uart/uart_devfs.h"
#include "printk.h"

static int handle_fs_call(noza_msg_t *msg)
{
    if (msg == NULL || msg->ptr == NULL) {
        return EINVAL;
    }

    size_t room = msg->size;
    if (room < sizeof(noza_fs_request_t) || room < sizeof(noza_fs_response_t)) {
        if (room >= sizeof(int32_t)) {
            ((noza_fs_response_t *)msg->ptr)->code = EMSGSIZE;
        }
        return EMSGSIZE;
    }

    noza_fs_request_t *req = (noza_fs_request_t *)msg->ptr;
    noza_fs_request_t req_copy = *req; // avoid overwriting request while building response
    noza_fs_response_t *resp = (noza_fs_response_t *)msg->ptr;
    memset(resp, 0, sizeof(*resp));

    int rc = fs_dispatch(msg->to_vid, &req_copy, &msg->identity, resp);
    if (resp->code == 0) {
        resp->code = rc;
    }
    return 0;
}

static int fs_main(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;

    uint32_t service_id = 0;
    int reg_ret = name_lookup_register(NOZA_FS_SERVICE_NAME, &service_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printk("fs: name register failed (%d)\n", reg_ret);
    }

    vfs_init();
    ramfs_init();
    int devfs_rc = devfs_init();
    if (devfs_rc != 0) {
        printk("fs: devfs init failed (%d)\n", devfs_rc);
    } else {
        uart_register_devfs();
    }

    // TODO: spawn worker threads when heavier VFS backends arrive.
    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) != 0) {
            continue;
        }
        handle_fs_call(&msg);
        noza_reply(&msg);
    }

    return 0;
}

static uint8_t fs_stack[2048];
void __attribute__((constructor(106))) fs_service_init(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(fs_main, fs_stack, sizeof(fs_stack));
}
