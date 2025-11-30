#pragma once

#include <stdint.h>
#include <stddef.h>
#include "noza_ipc.h"

// Service naming
#define NOZA_FS_SERVICE_NAME        "fs"
#define NOZA_FS_DEFAULT_PORT        "fs"    // placeholder for name_lookup registration

// Path and buffer limits
#define NOZA_FS_MAX_PATH            256     // includes trailing '\0'
#define NOZA_FS_MAX_NAME            64      // directory entry name limit
#define NOZA_FS_INLINE_IO_BYTES     256     // small I/O may be copied inline; larger uses caller buffers
#define NOZA_FS_UMASK_KEEP          0xFFFFFFFFu
#define NOZA_FS_OFFSET_CUR          0xFFFFFFFFu

// File types (mode high bits mirror POSIX S_IF*)
#define NOZA_FS_MODE_IFMT           0170000u
#define NOZA_FS_MODE_IFREG          0100000u
#define NOZA_FS_MODE_IFDIR          0040000u

typedef enum {
    NOZA_FS_OPEN = 1,
    NOZA_FS_CLOSE,
    NOZA_FS_READ,
    NOZA_FS_WRITE,
    NOZA_FS_LSEEK,
    NOZA_FS_STAT,
    NOZA_FS_FSTAT,
    NOZA_FS_OPENDIR,
    NOZA_FS_READDIR,
    NOZA_FS_CLOSEDIR,
    NOZA_FS_MKDIR,
    NOZA_FS_UNLINK,
    NOZA_FS_CHDIR,
    NOZA_FS_GETCWD,
    NOZA_FS_UMASK,
    NOZA_FS_CHMOD,
    NOZA_FS_CHOWN,
    NOZA_FS_MOUNT,
    NOZA_FS_UMOUNT,
} noza_fs_opcode_t;

typedef struct {
    uint32_t mode;       // permission bits + file type
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint32_t atime_sec;
    uint32_t mtime_sec;
    uint32_t ctime_sec;
    uint32_t nlink;
} noza_fs_attr_t;

typedef struct {
    uint32_t inode;
    uint32_t type;                       // NOZA_FS_MODE_IFREG / NOZA_FS_MODE_IFDIR etc.
    char name[NOZA_FS_MAX_NAME];
} noza_fs_dirent_t;

// Request/response are shared in the same buffer pointed by noza_msg_t->ptr.
// Kernel fills msg->identity (uid/gid/umask) automatically; FS must trust that value.

typedef struct {
    uint16_t opcode;                     // noza_fs_opcode_t
    uint16_t flags;                      // opcode-specific flags
    union {
        struct {                         // NOZA_FS_OPEN
            char path[NOZA_FS_MAX_PATH];
            uint32_t oflag;
            uint32_t mode;
        } open;
        struct {                         // NOZA_FS_CLOSE
            uint32_t handle;
        } close;
        struct {                         // NOZA_FS_READ / NOZA_FS_WRITE
            uint32_t handle;
            void *buf;                   // caller-owned buffer for zero-copy I/O
            uint32_t length;             // requested byte count
            uint32_t offset;             // optional offset for pread/pwrite (0xFFFFFFFF => use current)
        } rw;
        struct {                         // NOZA_FS_LSEEK
            uint32_t handle;
            int64_t offset;
            int32_t whence;
        } lseek;
        struct {                         // NOZA_FS_STAT / NOZA_FS_CHMOD / NOZA_FS_CHOWN / NOZA_FS_UNLINK / NOZA_FS_MKDIR / NOZA_FS_CHDIR / NOZA_FS_OPENDIR / NOZA_FS_UMOUNT
            char path[NOZA_FS_MAX_PATH];
            uint32_t mode;               // chmod/mkdir (ignored otherwise)
            uint32_t uid;                // chown (ignored otherwise)
            uint32_t gid;                // chown (ignored otherwise)
        } path;
        struct {                         // NOZA_FS_FSTAT / NOZA_FS_CLOSEDIR / NOZA_FS_READDIR
            uint32_t handle;
        } handle;
        struct {                         // NOZA_FS_GETCWD
            char buf[NOZA_FS_MAX_PATH];
            uint32_t buf_size;
        } cwd;
        struct {                         // NOZA_FS_UMASK
            uint32_t new_mask;           // set to NOZA_FS_UMASK_KEEP to query without changing
        } umask;
        struct {                         // NOZA_FS_MOUNT
            char source[NOZA_FS_MAX_PATH];
            char target[NOZA_FS_MAX_PATH];
            uint32_t fs_type;            // backend id
            void *data;                  // backend-specific payload
            uint32_t data_len;
        } mount;
    };
} noza_fs_request_t;

typedef struct {
    int32_t code;                        // 0 on success; errno (EACCES/EPERM/etc.) on failure
    union {
        struct {                         // NOZA_FS_OPEN / NOZA_FS_OPENDIR
            uint32_t handle;
        } open;
        struct {                         // NOZA_FS_READ / NOZA_FS_WRITE
            uint32_t length;             // bytes transferred
        } rw;
        struct {                         // NOZA_FS_LSEEK
            int64_t offset;
        } lseek;
        struct {                         // NOZA_FS_STAT / NOZA_FS_FSTAT
            noza_fs_attr_t attr;
        } stat;
        struct {                         // NOZA_FS_READDIR
            noza_fs_dirent_t entry;
            uint8_t at_end;              // 1 when no more entries
        } dir;
        struct {                         // NOZA_FS_GETCWD
            char path[NOZA_FS_MAX_PATH];
        } cwd;
        struct {                         // NOZA_FS_UMASK
            uint32_t old_mask;
        } umask;
    };
} noza_fs_response_t;

// I/O buffering strategy:
// - All requests ride inside noza_msg_t->ptr; large read/write payloads are exchanged
//   through caller-provided buffers referenced by the rw.buf pointer to avoid copies.
// - Short reads/writes (<= NOZA_FS_INLINE_IO_BYTES) may be serviced by copying
//   directly through the request buffer if backends prefer, but services should
//   honor rw.buf for zero-copy where possible.
// - Paths must be NUL-terminated and no longer than NOZA_FS_MAX_PATH bytes.
