#include <string.h>
#include "nozaos.h"
#include "noza_console_api.h"
#include "cmd_line.h"
#include "uart_io.h"
#include "service/name_lookup/name_lookup_client.h"
#include "service/irq/irq_client.h"
#include "noza_irq_defs.h"
#include "posix/errno.h"
#include "printk.h"

typedef struct uart_state {
    cmd_line_t uart_cmd;
    char line_buf[BUFLEN];
    volatile int line_ready;
    volatile int line_len;
    int irq_enabled;
    noza_msg_t pending_read_msg;
    console_msg_t *pending_read_cmsg;
} uart_state_t;

static void process_command(char *cmd_str, void *user_data)
{
    uart_state_t *state = (uart_state_t *)user_data;
    if (state == NULL) {
        return;
    }
    strncpy(state->line_buf, cmd_str, sizeof(state->line_buf) - 1);
    state->line_buf[sizeof(state->line_buf) - 1] = 0;
    state->line_len = (int)strlen(state->line_buf);
    state->line_ready = 1;
}

static void handle_irq_input(uart_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (;;) {
        int ch = state->uart_cmd.driver.getc();
        if (ch < 0) {
            break;
        }
        cmd_line_putc(&state->uart_cmd, ch);
    }
}

static void reply_pending_line_if_ready(uart_state_t *state)
{
    if (state == NULL) {
        return;
    }
    if (!state->line_ready || state->pending_read_cmsg == NULL) {
        return;
    }
    uint32_t copy_len = (state->line_len < (int)state->pending_read_cmsg->len) ? (uint32_t)state->line_len : state->pending_read_cmsg->len;
    memcpy(state->pending_read_cmsg->buf, state->line_buf, copy_len);
    state->pending_read_cmsg->len = copy_len;
    state->pending_read_cmsg->code = 0;
    noza_reply(&state->pending_read_msg);
    state->pending_read_cmsg = NULL;
    state->pending_read_msg.ptr = NULL;
    state->line_ready = 0;
    state->line_len = 0;
}

int uart_service_start(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;
    uart_state_t uart_state = {0};

    // init cmd line
    char_driver_t driver;
    extern void noza_char_init(char_driver_t *driver);
    noza_char_init(&driver);
    cmd_line_init(&uart_state.uart_cmd, &driver, process_command, &uart_state);

    uint32_t service_id = 0;
    int reg_ret = name_lookup_register(NOZA_CONSOLE_SERVICE_NAME, &service_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printk("console.io: name register failed (%d)\n", reg_ret);
    }

    // UART RX IRQ temporarily disabled; use polling mode.
    uart_state.irq_enabled = 0;

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) != 0) {
            continue;
        }
        if (msg.ptr && msg.size == sizeof(noza_irq_event_t)) {
            noza_irq_event_t evt = *(noza_irq_event_t *)msg.ptr;
            noza_reply(&msg); // unmask IRQ quickly
            if (evt.irq_id == NOZA_IRQ_UART0) {
                handle_irq_input(&uart_state);
                reply_pending_line_if_ready(&uart_state);
            }
            continue;
        }
        if (msg.ptr && msg.size == sizeof(console_msg_t)) {
            console_msg_t *cmsg = (console_msg_t *)msg.ptr;
            switch (cmsg->cmd) {
                case CONSOLE_CMD_WRITE: {
                    uint32_t n = cmsg->len;
                    if (n > sizeof(cmsg->buf)) n = sizeof(cmsg->buf);
                    for (uint32_t i = 0; i < n; i++) {
                        driver.putc(cmsg->buf[i]);
                    }
                    cmsg->code = 0;
                    noza_reply(&msg);
                    break;
                }
                case CONSOLE_CMD_READLINE: {
                    // if a line is already ready, serve it immediately
                    if (uart_state.line_ready) {
                        uint32_t copy_len = (uart_state.line_len < cmsg->len) ? (uint32_t)uart_state.line_len : cmsg->len;
                        memcpy(cmsg->buf, uart_state.line_buf, copy_len);
                        cmsg->len = copy_len;
                        cmsg->code = 0;
                        uart_state.line_ready = 0;
                        uart_state.line_len = 0;
                        noza_reply(&msg);
                        break;
                    }

                    if (!uart_state.irq_enabled) {
                        uart_state.line_ready = 0;
                        uart_state.line_len = 0;
                        while (!uart_state.line_ready) {
                            handle_irq_input(&uart_state);
                            noza_thread_sleep_ms(1, NULL);
                        }
                        uint32_t copy_len = (uart_state.line_len < cmsg->len) ? (uint32_t)uart_state.line_len : cmsg->len;
                        memcpy(cmsg->buf, uart_state.line_buf, copy_len);
                        cmsg->len = copy_len;
                        cmsg->code = 0;
                        uart_state.line_ready = 0;
                        uart_state.line_len = 0;
                        noza_reply(&msg);
                        break;
                    }

                    if (uart_state.pending_read_cmsg != NULL) {
                        cmsg->code = EBUSY;
                        noza_reply(&msg);
                        break;
                    }

                    uart_state.line_ready = 0;
                    uart_state.line_len = 0;
                    uart_state.pending_read_msg = msg;
                    uart_state.pending_read_cmsg = cmsg;
                    // prompt is handled by callers via console_write; no reply until line arrives
                    break;
                }
                default:
                    cmsg->code = 1;
                    noza_reply(&msg);
                    break;
            }
            continue;
        }
        noza_reply(&msg);
    }
    return 0;
}

static uint8_t uart_service_stack[1024 + sizeof(uart_state_t)];
void __attribute__((constructor(103))) uart_service_init(void *param, uint32_t pid)
{
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(uart_service_start, uart_service_stack, sizeof(uart_service_stack));
}
