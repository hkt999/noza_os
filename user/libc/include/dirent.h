#pragma once

#include <stddef.h>
#include "noza_fs.h"

// Minimal dirent definitions for Noza user space
typedef struct dirent {
    char d_name[NOZA_FS_MAX_NAME];
} dirent;

typedef struct DIR DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
