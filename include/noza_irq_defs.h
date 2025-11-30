#pragma once

#include <stdint.h>

// Reserved VID for the user-space IRQ service
// Pick a high value so auto-assigned thread IDs never collide before the
// service explicitly requests this VID during boot.
#define IRQ_SERVER_VID             65000u

// RP2040 exposes 26 NVIC-visible IRQs, but keep some slack for future boards
#define NOZA_RP2040_IRQ_COUNT      26
#define NOZA_MAX_IRQ_COUNT         32
#define NOZA_IRQ_SERVICE_NAME      "irq.svc"
#define NOZA_IRQ_UART0             20
#define NOZA_IRQ_SENDER_BASE       0xFFFF0000u
#define NOZA_IRQ_SENDER_MASK       0xFFFF0000u
#define NOZA_IRQ_SENDER_ID(irq_id) (NOZA_IRQ_SENDER_BASE | ((irq_id) & 0xFFFFu))
#define NOZA_IRQ_IS_KERNEL_VID(vid) (((vid) & NOZA_IRQ_SENDER_MASK) == NOZA_IRQ_SENDER_BASE)
#define NOZA_IRQ_SENDER_TO_ID(vid)  ((vid) & 0xFFFFu)

// Flags carried in the status field
#define NOZA_IRQ_STATUS_OVERFLOW   (1u << 0)

typedef struct {
    uint32_t irq_id;          // hardware IRQ number (0-based, matches doc/irq.txt)
    uint32_t timestamp_us;    // time captured in kernel when we latched the IRQ
    uint32_t status;          // NOZA_IRQ_STATUS_* flags | peripheral-specific bits
    uint32_t reserved;        // for future expansion (e.g. source core)
} noza_irq_event_t;

typedef enum {
    NOZA_IRQ_SERVICE_SUBSCRIBE = 1,
    NOZA_IRQ_SERVICE_UNSUBSCRIBE = 2
} noza_irq_service_cmd_t;

typedef struct {
    uint32_t cmd;
    uint32_t irq_id;
    uint32_t status;     // service writes errno-style result
    uint32_t reserved;
} noza_irq_service_msg_t;
