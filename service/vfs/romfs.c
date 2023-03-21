#include "vfs.h"
#include "romfs.h"
#include "setbit.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#define MAX_NUM_ROMFS   4
#define MAX_NUM_FILES   32

typedef struct serial_item_s {
	uint32_t name_offset;
	uint32_t data_offset;
} serial_item_t;

typedef struct read_dir_s {
	uint16_t dir_count;
	uint16_t file_count;
	serial_item_t item[];
} read_dir_t;

typedef struct rom_fd_s {
    uint32_t start;
    uint32_t cur_pos;
    uint32_t size;
} rom_fd_t;

typedef struct romfs_s {
    uint8_t *bin;
    size_t size;
    uint32_t fd_bits;
    rom_fd_t rom_fd[MAX_NUM_FILES];
} romfs_t;

static romfs_t ROMFS[MAX_NUM_ROMFS];
static uint32_t ROMFS_BITS = 0;

typedef struct strtok_noreplace_s {
    const char *last_token_end;
} strtok_noreplace_t;

static const char* strtok_noreplace(strtok_noreplace_t *t, const char* str, const char* delim) {
    //static const char* last_token_end = NULL;  // pointer to the end of the last token
    const char* token_start;  // pointer to the start of the current token
    const char* token_end;    // pointer to the end of the current token

    if (str != NULL) {
        // If str is not NULL, start parsing from the beginning of the string
        token_start = str + strspn(str, delim);  // skip leading delimiters
    } else if (t->last_token_end != NULL) {
        // If str is NULL, resume parsing from the end of the last token
        token_start = t->last_token_end;
        token_start += strspn(token_start, delim);  // skip delimiters after last token
    } else {
        // If str is NULL and there was no previous token, return NULL
        return NULL;
    }

    // Find the end of the current token
    token_end = token_start + strcspn(token_start, delim);
    if (*token_end != '\0') {
        // If the token is not the last one in the string, remember its end
        t->last_token_end = token_end + 1;
    } else {
        // If the token is the last one in the string, reset the pointer
        t->last_token_end = NULL;
    }

    return token_start;
}

static int romfd_open(vfs_t *vfs, const char *path, int oflag, int omode)
{
    romfs_t *romfs = (romfs_t *)vfs->context;
    read_dir_t *dir = (read_dir_t *)romfs->bin;
    strtok_noreplace_t sn;
    sn.last_token_end = NULL;

    // parse the path
    const char *name = strtok_noreplace(&sn, path, "/");
    while (sn.last_token_end != NULL) {
        for (int i=0; i<dir->dir_count; i++) {
            if (strcmp(name, (char *)(romfs->bin + dir->item[i].name_offset)) == 0) {
                // directory name found, extract the diretory name and move to next level
                dir = (read_dir_t *)(romfs->bin + dir->item[i].data_offset); // get corresponsing directory
                name = strtok_noreplace(&sn, NULL, "/");
                break; // break inner loop for entering the next level
            }
        }
    }

    // last token, find the corresponding file
    for (int i=dir->dir_count; i<dir->file_count+dir->dir_count; i++) {
        if (strcmp(name, (char *)(romfs->bin + dir->item[i].name_offset)) == 0) {
            // file found, setup the file descriptor structure
            int fd = find_first_set_bit(romfs->fd_bits);
            if (fd >= MAX_NUM_FILES) {
                printf("exceed max number of files (%d)\n", fd);
                return -1;
            }
            set_bit(&romfs->fd_bits, fd);
            rom_fd_t *romfd = &romfs->rom_fd[fd];
            romfd->size = *((uint32_t *)(romfs->bin + dir->item[i].data_offset));
            romfd->start = dir->item[i].data_offset + sizeof(uint32_t);
            romfd->cur_pos = 0;

            return fd;
        }
    }

    return -1;
}

static ssize_t romfd_read(vfs_t *vfs, int fd, void *buf, size_t nbyte)
{
    romfs_t *romfs = (romfs_t *)vfs->context;
    rom_fd_t *romfd = &romfs->rom_fd[fd];
    if (romfd->cur_pos + nbyte > romfd->size) {
        nbyte = romfd->size - romfd->cur_pos;
    }
    memcpy(buf, romfs->bin + romfd->start + romfd->cur_pos, nbyte);
    romfd->cur_pos += nbyte;

    return nbyte;
}

static ssize_t romfd_write(vfs_t *vfs, int fd, const void *buf, size_t nbyte)
{
    return -1; // not supported
}

static int romfd_close(vfs_t *vfs, int fd)
{
    romfs_t *romfs = (romfs_t *)vfs->context;
    rom_fd_t *romfd = &romfs->rom_fd[fd];

    memset(romfd, 0, sizeof(rom_fd_t));
    clear_bit(&romfs->fd_bits, fd); // clear the bit

    return 0;
}

static off_t romfd_lseek(vfs_t *vfs, int fd, off_t offset, int whence)
{
    romfs_t *romfs = (romfs_t *)vfs->context;
    rom_fd_t *romfd = &romfs->rom_fd[fd];
    switch (whence) {
        case SEEK_SET:
            romfd->cur_pos = offset;
            break;
        case SEEK_CUR:
            romfd->cur_pos += offset;
            break;
        case SEEK_END:
            romfd->cur_pos = romfd->size + offset;
            break;
    }

    if (romfd->cur_pos > romfd->size)
        romfd->cur_pos = romfd->size;

    return (off_t)romfd->cur_pos;
}

static int romfd_stat(vfs_t *vfs, const char *path, struct sys_stat *buf)
{
    romfs_t *romfs = (romfs_t *)vfs->context;
    read_dir_t *dir = (read_dir_t *)romfs->bin;
    strtok_noreplace_t sn;
    sn.last_token_end = NULL;

    printf("romfd_open path=[%s] romfs->bin=%p\n", path, romfs->bin);
    // parse the path
    const char *name = strtok_noreplace(&sn, path, "/");
    while (sn.last_token_end != NULL) {
        for (int i=0; i<dir->dir_count; i++) {
            if (strcmp(name, (char *)(romfs->bin + dir->item[i].name_offset)) == 0) {
                // directory name found, extract the diretory name and move to next level
                dir = (read_dir_t *)(romfs->bin + dir->item[i].data_offset); // get corresponsing directory
                name = strtok_noreplace(&sn, NULL, "/");
                break; // break inner loop for entering the next level
            }
        }
    }
    // TODO: finish the function !

    return -1;
}

void romfs_init(vfs_t *vfs, uint8_t *p, size_t sz)
{
    int index = find_first_zero_bit(ROMFS_BITS);
    if (index >= MAX_NUM_ROMFS) {
        printf("fatal error ! not enough romfs\n");
        // TODO: error handling
        return;
    }

    romfs_t *romfs = &ROMFS[index];
    romfs->bin = p;
    romfs->size = sz;
    romfs->fd_bits = (uint32_t)-1;
    vfs->context = (void *)romfs;
    vfs->open = romfd_open;
    vfs->read = romfd_read;
    vfs->write = romfd_write;
    vfs->lseek = romfd_lseek;
    vfs->close = romfd_close;
    vfs->stat = romfd_stat;
    set_bit(&ROMFS_BITS, index);
}
