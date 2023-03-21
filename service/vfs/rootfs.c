#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "vfs.h"
#include "romfs.h"
#include "setbit.h"
#include "rootfs.h"
#include "os_debug.h"

#define MAX_NUM_MOUNTS  8
#define MAX_PATH_LEN    256

static vfs_t ROOTFS;
extern uint8_t rootfs_bin[]; // the binary data of the romfs
extern unsigned int rootfs_bin_len; // the length of the romfs binary data

typedef struct mount_rec_s {
    char *path;
    int path_len;
    vfs_t *vfs;
    void *next;
} mount_rec_t;

typedef struct open_item_s {
    mount_rec_t *mount_rec;
    int vfs_fd;
} open_item_t;

static open_item_t OPEN_FILES[MAX_OPEN_FILES];
static uint32_t OPEN_FILE_BITS = 0;

typedef struct mount_mgr_s {
    mount_rec_t points[MAX_NUM_MOUNTS];
    mount_rec_t *first_free;
    mount_rec_t *first_occupied;
} mount_mgr_t;

/**********************/

static mount_mgr_t MOUNT_MGR; // the mount manager
void mount_mgr_init(mount_mgr_t *mgr)
{
    memset(mgr, 0, sizeof(mount_mgr_t));
    for (int i=0; i<MAX_NUM_MOUNTS-1; i++) {
        mgr->points[i].next = &mgr->points[i+1];
    }
    mgr->first_free = &mgr->points[0];
    mgr->first_occupied = NULL;
    memset(OPEN_FILES, 0, sizeof(OPEN_FILES));
}

static inline void mount_mgr_add_occupied(mount_mgr_t *mgr)
{
    mount_rec_t *rec = mgr->first_free;
    mgr->first_free = rec->next;
    rec->next = mgr->first_occupied;
    mgr->first_occupied = rec;
}

/**********************/

void init_rootfs()
{
    mount_mgr_init(&MOUNT_MGR);

    memset(&ROOTFS, 0, sizeof(vfs_t));
    romfs_init(&ROOTFS, rootfs_bin, rootfs_bin_len);
    rootfs_mount("/", &ROOTFS);
}

int rootfs_mount(const char *path, vfs_t *vfs)
{
    if (MOUNT_MGR.first_occupied == NULL) {
        // the first mount point must be the rootfs
        if (strncmp(path, "/", MAX_PATH_LEN)!=0) {
            return -1; // fail
        }
        mount_rec_t *rec = MOUNT_MGR.first_free;
        rec->path = strndup(path, MAX_PATH_LEN); // TODO: check the memory allocation
        rec->path_len = strlen(rec->path);
        rec->vfs = vfs;
        mount_mgr_add_occupied(&MOUNT_MGR);
        return 0; // success
    } else {
        mount_rec_t *rec = MOUNT_MGR.first_free;
        if (rec == NULL) {
            return -1; // fail
        }
        // check if the input path is already mounted or not
        mount_rec_t *p = MOUNT_MGR.first_occupied;
        while (p != NULL) {
            if (strncmp(path, p->path, MAX_PATH_LEN)==0) {
                return -1; // already mounted
            }
            p = p->next;
        }

        struct sys_stat statbuf;
        if ((ROOTFS.stat(&ROOTFS, path, &statbuf) == 0) && ((statbuf.st_mode & S_IFDIR) != 0)) {
            // record the mount point
            rec->path = strndup(path, MAX_PATH_LEN); // TODO: check the memory allocation
            if (rec->path==NULL) {
                // TODO: show error message
                return -1;
            }
            rec->vfs = vfs;
            rec->path_len = strlen(rec->path);
            mount_mgr_add_occupied(&MOUNT_MGR);
        }

        return 0;
    }
}

int rootfs_unmount(const char *path)
{
    mount_rec_t *rec = MOUNT_MGR.first_occupied;
    while (rec) {
        if ((rec->path != NULL) && (strncmp(path, rec->path, MAX_PATH_LEN)==0)) {
            free(rec->path);
            rec->path = NULL;
            rec->vfs = NULL;
            rec->next = MOUNT_MGR.first_free;
            MOUNT_MGR.first_free = rec;
            return 0;
        }
        rec = rec->next;
    }

    return -1; // not found
}

static mount_rec_t *rootfs_match_mount(const char *path)
{
    mount_rec_t *mark = NULL;
    mount_rec_t *rec = MOUNT_MGR.first_occupied;
    while (rec != NULL) {
        if (strncmp(path, rec->path, rec->path_len)==0) {
            if (mark == NULL) {
                mark = rec;
            } else {
                if (rec->path_len > mark->path_len) {
                    mark = rec;
                }
            }
        }
        rec = rec->next;
    }

    return mark;
}

// rootfs io api
int rootfs_open(const char *path, int flags, int mode)
{
    int open_fd = find_first_zero_bit(OPEN_FILE_BITS);
    if (open_fd<0) {
        printk("rootfs_open: no more fd available\n");
        return -1; // not enough fd
    }
    mount_rec_t *mount = rootfs_match_mount(path);
    if (mount) {
        int fd =  mount->vfs->open(mount->vfs, path, flags, mode);
        if (fd>=0) {
            OPEN_FILES[open_fd].mount_rec = mount;
            OPEN_FILES[open_fd].vfs_fd = fd;
            set_bit(&OPEN_FILE_BITS, open_fd);
            return open_fd;
        }
    }

    return -1;
}

int rootfs_close(int fd)
{
    vfs_t *vfs = OPEN_FILES[fd].mount_rec->vfs;
    int vfs_fd = OPEN_FILES[fd].vfs_fd;
    vfs->close(vfs, vfs_fd);
    clear_bit(&OPEN_FILE_BITS, fd); // clear the root fd

    return 0;
}

int rootfs_read(int fd, void *buf, size_t size)
{
    vfs_t *vfs = OPEN_FILES[fd].mount_rec->vfs;
    int vfs_fd = OPEN_FILES[fd].vfs_fd;

    return vfs->read(vfs, vfs_fd, buf, size);
}

int rootfs_write(int fd, const void *buf, size_t size)
{
    vfs_t *vfs = OPEN_FILES[fd].mount_rec->vfs;
    int vfs_fd = OPEN_FILES[fd].vfs_fd;

    return vfs->write(vfs, vfs_fd, buf, size);
}

int rootfs_lseek(int fd, off_t offset, int whence)
{
    vfs_t *vfs = OPEN_FILES[fd].mount_rec->vfs;
    int vfs_fd = OPEN_FILES[fd].vfs_fd;

    return vfs->lseek(vfs, vfs_fd, offset, whence);
}

int rootfs_stat(const char *path, struct sys_stat *buf)
{
    mount_rec_t *mount = rootfs_match_mount(path);
    if (mount) {
        return mount->vfs->stat(mount->vfs, path, buf);
    }

    return -1; // fail
}