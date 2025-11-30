#include <stddef.h>
#include "posix/errno.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "nozaos.h"
#include "service/console/console_io_client.h"

static int console_write_wrapper(const void *buf, size_t count) {
    int ret = console_write((const char *)buf, (uint32_t)count);
    if (ret != 0) {
        noza_set_errno(ret);
        return -1;
    }
    return (int)count;
}

static int console_read_wrapper(void *buf, size_t count) {
    uint32_t out_len = 0;
    int ret = console_readline((char *)buf, (uint32_t)count, &out_len);
    if (ret != 0) {
        noza_set_errno(ret);
        return -1;
    }
    return (int)out_len;
}

int _write(int fd, const void *buf, size_t count) {
    if (buf == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return console_write_wrapper(buf, count);
    }
    noza_set_errno(ENOSYS);
    return -1;
}

int _read(int fd, void *buf, size_t count) {
    if (buf == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDIN_FILENO) {
        return console_read_wrapper(buf, count);
    }
    noza_set_errno(ENOSYS);
    return -1;
}

off_t _lseek(int fd, off_t offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    noza_set_errno(ENOSYS);
    return (off_t)-1;
}

int _close(int fd) {
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return 0;
    }
    noza_set_errno(ENOSYS);
    return -1;
}

int _fstat(int fd, struct stat *st) {
    if (st == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFCHR;
        return 0;
    }
    noza_set_errno(ENOSYS);
    return -1;
}

int _isatty(int fd) {
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return 1;
    }
    noza_set_errno(ENOTTY);
    return 0;
}

int _open(const char *path, int flags, int mode) {
    (void)path;
    (void)flags;
    (void)mode;
    noza_set_errno(ENOSYS);
    return -1;
}

int _unlink(const char *path) {
    (void)path;
    noza_set_errno(ENOSYS);
    return -1;
}
