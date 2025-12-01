#pragma once

#include <stdint.h>
#include "vfs.h"

typedef struct devfs_device_ops {
    int (*open)(void *ctx, uint32_t oflag, uint32_t mode, void **dev_handle);
    int (*close)(void *dev_handle);
    int (*read)(void *dev_handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
    int (*write)(void *dev_handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
    int (*lseek)(void *dev_handle, int64_t offset, int32_t whence, int64_t *new_off);
    int (*ioctl)(void *dev_handle, uint32_t req, void *arg);
} devfs_device_ops_t;

int devfs_init(void);
int devfs_register_char(const char *name, uint32_t mode, const devfs_device_ops_t *ops, void *ctx);
