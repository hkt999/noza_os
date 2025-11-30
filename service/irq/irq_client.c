#include "irq_client.h"
#include "service/name_lookup/name_lookup_client.h"
#include "nozaos.h"
#include "posix/errno.h"

static uint32_t irq_service_id;
static uint32_t irq_service_vid;

static int ensure_irq_service(uint32_t *vid_out)
{
    if (irq_service_vid != 0) {
        if (vid_out) {
            *vid_out = irq_service_vid;
        }
        return 0;
    }

    uint32_t vid = 0;
    int ret = name_lookup_resolve(NOZA_IRQ_SERVICE_NAME, &irq_service_id, &vid);
    if (ret == NAME_LOOKUP_OK) {
        irq_service_vid = vid;
        if (vid_out) {
            *vid_out = vid;
        }
        return 0;
    }
    return ret;
}

static int send_cmd(uint32_t irq_id, uint32_t cmd)
{
    uint32_t target_vid = 0;
    int ensure = ensure_irq_service(&target_vid);
    if (ensure != 0) {
        return ensure;
    }

    noza_irq_service_msg_t msg = {
        .cmd = cmd,
        .irq_id = irq_id,
        .status = 0,
        .reserved = 0
    };
    noza_msg_t m = {
        .to_vid = target_vid,
        .ptr = &msg,
        .size = sizeof(msg)
    };
    int ret = noza_call(&m);
    if (ret != 0) {
        irq_service_vid = 0;
        return ret;
    }
    return (int)msg.status;
}

int irq_service_subscribe(uint32_t irq_id)
{
    return send_cmd(irq_id, NOZA_IRQ_SERVICE_SUBSCRIBE);
}

int irq_service_unsubscribe(uint32_t irq_id)
{
    return send_cmd(irq_id, NOZA_IRQ_SERVICE_UNSUBSCRIBE);
}
