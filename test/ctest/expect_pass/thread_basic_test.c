#include <stdio.h>
#include "flmalloc.h"
#include "debug.h"
#include <pthread.h>
#include <string.h>

//extern fl_metadata_t thread_mds[MAX_THREAD];


void* run(uintptr_t thread_id) {
    setbuf(stderr, NULL);
    fprintf(stderr, "current id: %lu\n", pthread_self());
    void* ptr;
    ptr = fl_malloc(32 * (thread_id + 1));
    fl_free(ptr);

    return NULL;
}

int main() {
    size_t nthreads = 5;
    pthread_t* threads = (pthread_t*)fl_malloc(sizeof(pthread_t) * nthreads);
    memset(threads, 0, sizeof(pthread_t) * nthreads);
    fprintf(stderr, "current id: %lu\n", pthread_self());

    for (uintptr_t i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, (void *(*)(void *)) &run, (void*)i);
        fprintf(stderr, "enter thread %lu\n", threads[i]);
    }
    for (size_t i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    fl_free(threads);

//    pthread_create(&thread, NULL, run, NULL);
//    pthread_join(thread, NULL);

    return 0;
}

