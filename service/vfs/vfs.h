#pragma once

#include <stdint.h>
#include <unistd.h>
#include "syscall.h"

#define MAX_OPEN_FILES 32

typedef struct vfs_s vfs_t;
struct vfs_s {
	uint32_t fd_bits; // TODO: extend this to more bits
	void *context;

	int (*open)(vfs_t *vfs, const char *path, int oflag, int omode);
	ssize_t (*read)(vfs_t *vfs, int fd, void *buf, size_t nbyte);
	ssize_t (*write)(vfs_t *vfs, int fd, const void *buf, size_t nbyte);
	int (*close)(vfs_t *vfs, int fd);
	off_t (*lseek)(vfs_t *vfs, int fd, off_t offset, int whence);
	int (*stat)(vfs_t *vfs, const char *path, struct sys_stat *buf);
};

int vfs_get_free_fd(vfs_t *vfs);
void vfs_clear_free_fd(vfs_t *vfs, int fd);