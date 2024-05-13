#ifndef UNIALLOC_FLMALLOC_H
#define UNIALLOC_FLMALLOC_H

#include "defines.h"
#ifdef __cplusplus
extern "C" {
#endif

void *fl_malloc(size_t size);

void fl_free(void *ptr);

void *fl_realloc(void *ptr, size_t size);

void *fl_calloc(size_t nmemb, size_t size);

void*fl_memalign(size_t alignment, size_t size);

int fl_posix_memalign(void**, size_t, size_t);

void* fl_aligned_alloc(size_t, size_t);

#ifdef __cplusplus
}
#endif
#endif //UNIALLOC_FLMALLOC_H
