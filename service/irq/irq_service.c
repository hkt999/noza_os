#include <string.h>
#include "nozaos.h"
#include "posix/errno.h"
#include "service/name_lookup/name_lookup_client.h"
#include "noza_irq_defs.h"
#include "printk.h"

static uint32_t subscriber_vid[NOZA_RP2040_IRQ_COUNT];

static int handle_command(noza_irq_service_msg_t *cmd, uint32_t sender_vid)
{
    if (cmd == NULL) {
        return EINVAL;
    }
    if (cmd->irq_id >= NOZA_RP2040_IRQ_COUNT) {
        cmd->status = EINVAL;
        return 0;
    }

    uint32_t current = subscriber_vid[cmd->irq_id];
    switch (cmd->cmd) {
        case NOZA_IRQ_SERVICE_SUBSCRIBE:
            if (current == 0 || current == sender_vid) {
                subscriber_vid[cmd->irq_id] = sender_vid;
                cmd->status = 0;
            } else {
                cmd->status = EBUSY;
            }
            break;

        case NOZA_IRQ_SERVICE_UNSUBSCRIBE:
            if (current == sender_vid) {
                subscriber_vid[cmd->irq_id] = 0;
                cmd->status = 0;
            } else {
                cmd->status = EPERM;
            }
            break;

        default:
            cmd->status = EINVAL;
            break;
    }
    return 0;
}

static void forward_event(noza_irq_event_t *event)
{
    if (event == NULL) {
        return;
    }
    if (event->irq_id >= NOZA_RP2040_IRQ_COUNT) {
        return;
    }
    uint32_t target = subscriber_vid[event->irq_id];
    if (target == 0) {
        return;
    }

    noza_irq_event_t copy = *event;
    noza_msg_t msg = {
        .to_vid = target,
        .ptr = (void *)&copy,
        .size = sizeof(copy)
    };
    if (noza_call(&msg) != 0) {
        subscriber_vid[event->irq_id] = 0;
    }
}

static int do_irq_service(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;

    uint32_t service_id = 0;
    int reg_ret = name_lookup_register(NOZA_IRQ_SERVICE_NAME, &service_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printk("irq: name register failed (%d)\n", reg_ret);
    }

    memset(subscriber_vid, 0, sizeof(subscriber_vid));

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) != 0) {
            continue;
        }
        if (msg.ptr == NULL) {
            noza_reply(&msg);
            continue;
        }

        if (NOZA_IRQ_IS_KERNEL_VID(msg.to_vid) && msg.size == sizeof(noza_irq_event_t)) {
            noza_irq_event_t event = *(noza_irq_event_t *)msg.ptr;
            noza_reply(&msg); // unmask in kernel
            forward_event(&event);
        } else if (msg.size == sizeof(noza_irq_service_msg_t)) {
            noza_irq_service_msg_t *cmd = (noza_irq_service_msg_t *)msg.ptr;
            handle_command(cmd, msg.to_vid);
            noza_reply(&msg);
        } else {
            noza_reply(&msg);
        }
    }
    return 0;
}

static uint8_t irq_server_stack[1024];
void __attribute__((constructor(105))) irq_service_init(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    extern void noza_add_service_with_vid(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size, uint32_t reserved_vid);
    noza_add_service_with_vid(do_irq_service, irq_server_stack, sizeof(irq_server_stack), IRQ_SERVER_VID);
}
