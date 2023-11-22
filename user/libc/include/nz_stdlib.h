#pragma once

void *nz_malloc(size_t size);
void nz_free(void *ptr);
char *nz_strdup(char *s);

// wrapper
#define malloc(x)	nz_malloc(x)
#define free(x)		nz_free(x)
#define strdup(x)	nz_strdup(x)

