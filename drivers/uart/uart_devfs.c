#include <string.h>
#include "posix/errno.h"
#include "service/fs/devfs.h"
#include "drivers/uart/uart_io_client.h"
#include "noza_uart.h"
#include "noza_fs.h"
#include "printk.h"

static int uart_dev_open(void *ctx, uint32_t oflag, uint32_t mode, void **dev_handle)
{
    (void)ctx;
    (void)oflag;
    (void)mode;
    (void)dev_handle;
    return 0;
}

static int uart_dev_close(void *dev_handle)
{
    (void)dev_handle;
    return 0;
}

static int uart_dev_write(void *dev_handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)dev_handle;
    (void)offset;
    if (out_len == NULL) {
        return EINVAL;
    }
    if (buf == NULL || len == 0) {
        *out_len = 0;
        return 0;
    }
    noza_uart_write((const char *)buf, len);
    *out_len = len;
    return 0;
}

static int uart_dev_read(void *dev_handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)dev_handle;
    if (out_len == NULL) {
        return EINVAL;
    }
    if (buf == NULL || len == 0) {
        *out_len = 0;
        return 0;
    }
    if (offset != NOZA_FS_OFFSET_CUR && offset != 0) {
        return ESPIPE;
    }
    uint32_t got = 0;
    int rc = console_readline((char *)buf, len, &got);
    if (rc != 0) {
        return rc;
    }
    *out_len = got;
    return 0;
}

static int uart_dev_lseek(void *dev_handle, int64_t offset, int32_t whence, int64_t *new_off)
{
    (void)dev_handle;
    (void)offset;
    (void)whence;
    (void)new_off;
    return ESPIPE;
}

static const devfs_device_ops_t UART_DEV_OPS = {
    .open = uart_dev_open,
    .close = uart_dev_close,
    .read = uart_dev_read,
    .write = uart_dev_write,
    .lseek = uart_dev_lseek,
    .ioctl = NULL,
};

void uart_register_devfs(void)
{
    static int registered = 0;
    if (registered) {
        return;
    }
    int rc = devfs_register_char("ttyS0", 0666, &UART_DEV_OPS, NULL);
    if (rc != 0) {
        printk("[devfs] register /dev/ttyS0 failed rc=%d\n", rc);
        return;
    }
    printk("[devfs] /dev/ttyS0 ready\n");
    registered = 1;
}
