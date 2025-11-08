#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *tlsf_t;

tlsf_t tlsf_create_with_pool(void *mem, size_t bytes);
void tlsf_destroy(tlsf_t tlsf);

void *tlsf_malloc(tlsf_t tlsf, size_t bytes);
void tlsf_free(tlsf_t tlsf, void *ptr);
void *tlsf_realloc(tlsf_t tlsf, void *ptr, size_t bytes);
void *tlsf_memalign(tlsf_t tlsf, size_t alignment, size_t bytes);

size_t tlsf_used_size(tlsf_t tlsf);
size_t tlsf_free_size(tlsf_t tlsf);

#ifdef __cplusplus
}
#endif
