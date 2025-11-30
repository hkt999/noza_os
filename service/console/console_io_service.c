#include <string.h>
#include <stdio.h>
#include "nozaos.h"
#include "noza_console_api.h"
#include "cmd_line.h"
#include "console_io.h"
#include "service/name_lookup/name_lookup_client.h"
#include "service/irq/irq_client.h"
#include "noza_irq_defs.h"
#include "posix/errno.h"

static cmd_line_t console_cmd;
static char line_buf[BUFLEN];
static volatile int line_ready;
static volatile int line_len;
static int irq_enabled;
static noza_msg_t pending_read_msg;
static console_msg_t *pending_read_cmsg;

static void process_command(char *cmd_str, void *user_data)
{
    (void)user_data;
    strncpy(line_buf, cmd_str, sizeof(line_buf) - 1);
    line_buf[sizeof(line_buf) - 1] = 0;
    line_len = strlen(line_buf);
    line_ready = 1;
}

static void handle_irq_input(void)
{
    for (;;) {
        int ch = console_cmd.driver.getc();
        if (ch < 0) {
            break;
        }
        cmd_line_putc(&console_cmd, ch);
    }
}

static void reply_pending_line_if_ready(void)
{
    if (!line_ready || pending_read_cmsg == NULL) {
        return;
    }
    uint32_t copy_len = (line_len < (int)pending_read_cmsg->len) ? (uint32_t)line_len : pending_read_cmsg->len;
    memcpy(pending_read_cmsg->buf, line_buf, copy_len);
    pending_read_cmsg->len = copy_len;
    pending_read_cmsg->code = 0;
    noza_reply(&pending_read_msg);
    pending_read_cmsg = NULL;
    pending_read_msg.ptr = NULL;
    line_ready = 0;
    line_len = 0;
}

int console_service_start(void *param, uint32_t pid)
{
    (void)param;
    (void)pid;

    // init cmd line
    char_driver_t driver;
    extern void noza_char_init(char_driver_t *driver);
    noza_char_init(&driver);
    cmd_line_init(&console_cmd, &driver, process_command, NULL);

    uint32_t service_id = 0;
    int reg_ret = name_lookup_register(NOZA_CONSOLE_SERVICE_NAME, &service_id);
    if (reg_ret != NAME_LOOKUP_OK) {
        printf("console.io: name register failed (%d)\n", reg_ret);
    }

    irq_enabled = (irq_service_subscribe(NOZA_IRQ_UART0) == 0);
    if (!irq_enabled) {
        printf("console.io: irq subscribe failed, fallback to polling\n");
    }

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) != 0) {
            continue;
        }
        if (msg.ptr && msg.size == sizeof(noza_irq_event_t)) {
            noza_irq_event_t evt = *(noza_irq_event_t *)msg.ptr;
            noza_reply(&msg); // unmask IRQ quickly
            if (evt.irq_id == NOZA_IRQ_UART0) {
                handle_irq_input();
                reply_pending_line_if_ready();
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
                    if (line_ready) {
                        uint32_t copy_len = (line_len < cmsg->len) ? line_len : cmsg->len;
                        memcpy(cmsg->buf, line_buf, copy_len);
                        cmsg->len = copy_len;
                        cmsg->code = 0;
                        line_ready = 0;
                        line_len = 0;
                        noza_reply(&msg);
                        break;
                    }

                    if (!irq_enabled) {
                        line_ready = 0;
                        line_len = 0;
                        while (!line_ready) {
                            handle_irq_input();
                            noza_thread_sleep_ms(1, NULL);
                        }
                        uint32_t copy_len = (line_len < cmsg->len) ? line_len : cmsg->len;
                        memcpy(cmsg->buf, line_buf, copy_len);
                        cmsg->len = copy_len;
                        cmsg->code = 0;
                        noza_reply(&msg);
                        break;
                    }

                    if (pending_read_cmsg != NULL) {
                        cmsg->code = EBUSY;
                        noza_reply(&msg);
                        break;
                    }

                    line_ready = 0;
                    line_len = 0;
                    pending_read_msg = msg;
                    pending_read_cmsg = cmsg;
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

static uint8_t console_service_stack[1024];
void __attribute__((constructor(103))) console_service_init(void *param, uint32_t pid)
{
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(console_service_start, console_service_stack, sizeof(console_service_stack));
}
