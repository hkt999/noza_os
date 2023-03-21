#pragma once

#include <unistd.h>

typedef struct {
    uint32_t pid;
} process_t;

struct sys_stat {
    uint32_t st_mode;
};

#define S_IFMT     0170000   // bit mask for the file type bit fields
#define S_IFSOCK   0140000   // socket
#define S_IFLNK    0120000   // symbolic link
#define S_IFREG    0100000   // regular file
#define S_IFBLK    0060000   // block device
#define S_IFDIR    0040000   // directory
#define S_IFCHR    0020000   // character device
#define S_IFIFO    0010000   // FIFO


int     service_open(process_t *p, const char *path, int flags, int mode);
int     service_close(process_t *p, int fd);
size_t  service_read(process_t *p, int fd, void *buf, size_t size);
size_t  service_write(process_t *p, int fd, const void *buf, int count);
off_t   service_lseek(process_t *p, int fd, off_t offset, int whence);
int     service_stat(process_t *p, const char *path, struct sys_stat *buf);

