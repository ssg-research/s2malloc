//
// https://stackoverflow.com/questions/6083337/overriding-malloc-using-the-ld-preload-mechanism
//

#ifdef GLIBC_OVERRIDE
#define _GNU_SOURCE

#include <stdio.h>
#include "../include/flmalloc.h"
#include <dlfcn.h>


static void* fl_valloc(size_t s) {
    fprintf(stderr, "Not supported with fl_valloc %zu\n", s);
    return NULL;
}

static void* fl_pvalloc(size_t s) {
    fprintf(stderr, "Not supported with fl_pvalloc %zu\n", s);
    return NULL;
}

static void* fl_alloca(size_t s) {
    fprintf(stderr, "Not supported with fl_alloca %zu\n", s);
    return NULL;
}

static void* (*real_malloc)(size_t)=fl_malloc;
static void (*real_free)(void*)=fl_free;
static void* (*real_realloc)(void*, size_t)=fl_realloc;
static void* (*real_memalign)(size_t, size_t)=fl_memalign;
static void* (*real_calloc)(size_t, size_t)=fl_calloc;
static void* (*real_valloc)(size_t) = fl_valloc;
static void* (*real_aligned_alloc)(size_t, size_t) = fl_aligned_alloc;
static void* (*real_pvalloc)(size_t) = fl_pvalloc;
static void* (*real_alloca)(size_t) = fl_alloca;
static int (*real_posix_memalign)(void**, size_t, size_t) = fl_posix_memalign;


static void mtrace_init(void)
{
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_free = dlsym(RTLD_NEXT, "free");
    if (NULL == real_free) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_realloc = dlsym(RTLD_NEXT, "realloc");
    if (NULL == real_realloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_memalign = dlsym(RTLD_NEXT, "memalign");
    if (NULL == real_memalign) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_calloc = dlsym(RTLD_NEXT, "calloc");
    if (NULL == real_calloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_valloc = dlsym(RTLD_NEXT, "valloc");
    real_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
    real_pvalloc = dlsym(RTLD_NEXT, "pvalloc");
    real_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
    real_alloca = dlsym(RTLD_NEXT, "alloca");
}


void *malloc(size_t size)
{
    if(real_malloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
    p = real_malloc(size);
#ifdef WRAPPER_INFO
    fprintf(stderr, "wrapper malloc(%zu) = %p\n", size, p);
#endif
    return p;
}

void free(void* ptr)
{
    if(real_free==NULL) {
        mtrace_init();
    }

    void *p = NULL;
#ifdef WRAPPER_INFO
    fprintf(stderr, "wrapper free(%p)\n", ptr);
#endif
    real_free(ptr);
}


void* realloc(void* ptr, size_t size)
{
    if(real_realloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
#ifdef WRAPPER_INFO
    fprintf(stderr, "wrapper realloc(%p, %zu)\n", ptr, size);
#endif
    return real_realloc(ptr, size);
}

void* memalign(size_t alignment, size_t size)
{
    if(real_memalign==NULL) {
        mtrace_init();
    }

    void *p = NULL;
#ifdef WRAPPER_INFO
    fprintf(stderr, "wrapper memalign(%zu, %zu)\n", alignment, size);
#endif
    return real_memalign(alignment, size);
}

void* calloc(size_t nmemb, size_t size)
{
    if(real_calloc==NULL) {
        mtrace_init();
    }

    void *p = NULL;
#ifdef WRAPPER_INFO
    fprintf(stderr, "wrapper calloc(%zu, %zu)\n", nmemb, size);
#endif
    return real_calloc(nmemb, size);
}

void* valloc(size_t s) {
    return fl_valloc(s);
}

void* aligned_alloc(size_t s, size_t s2) {
    return fl_aligned_alloc(s, s2);
}

void* pvalloc(size_t s) {
    return fl_pvalloc(s);
}

void *alloca(size_t s) {
    return fl_alloca(s);
}

int posix_memalign(void** ptr, size_t s, size_t s2) {
//    print_summary();
//    return NULL;
    return fl_posix_memalign(ptr, s, s2);
}

#endif