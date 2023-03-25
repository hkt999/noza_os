#pragma once
#include <stdbool.h>

#define MAP_PRIVATE         0x0001
#define MAP_ANONYMOUS       0x0002
#define MAP_UNINITIALIZED   0x0004
#define MAP_FAILED          ((void *)-1)

#define PROT_READ   0x0001
#define PROT_WRITE  0x0002

typedef uint32_t offset;
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t len);

#define POSIX_MADV_DONTNEED 0
int posix_madvise(void *addr, size_t len, int advice);