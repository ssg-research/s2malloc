#ifndef UNIALLOC_THREADS_H
#define UNIALLOC_THREADS_H

#include <stdint.h>
#include <pthread.h>
#include "pcg.h"
#include "haesni.h"
#include "tlb.h"
#include "metadata_pool.h"

extern metadata_pool_t* metadata_pool_ptr;
extern fl_metadata_all_t* thread_mds;
volatile extern atomic32_t* thread_bump;
extern __thread size_t thread_id;
extern __thread size_t tid;

static pthread_mutex_t thread_init_lock;

static inline uintptr_t get_thread_id(void);
static inline void init_process();
static inline void init_thread();
static inline void exit_process();
static inline void exit_thread();


//! Fast thread ID
// https://github.com/mjansson/rpmalloc
inline uintptr_t get_thread_id(void) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__CYGWIN__)
    uintptr_t tid;
#  if defined(__i386__)
    __asm__("movl %%gs:0, %0" : "=r" (tid) : : );
#  elif defined(__x86_64__)
    #    if defined(__MACH__)
	__asm__("movq %%gs:0, %0" : "=r" (tid) : : );
#    else
	__asm__("movq %%fs:0, %0" : "=r" (tid) : : );
#    endif
#  elif defined(__arm__)
    __asm__ volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r" (tid));
#  elif defined(__aarch64__)
#    if defined(__MACH__)
    // tpidr_el0 likely unused, always return 0 on iOS
	__asm__ volatile ("mrs %0, tpidrro_el0" : "=r" (tid));
#    else
    __asm__ volatile ("mrs %0, tpidr_el0" : "=r" (tid));
#    endif
#  else
    tid = (uintptr_t)((void*)get_thread_heap_raw());
#  endif
    return tid;
#else
    return (uintptr_t)((void*)get_thread_heap_raw());
#endif
}

static inline void init_thread() {
    size_t cur_tid = get_thread_id();
    for (size_t i = 0; i <= *thread_bump; i++) {
        if (thread_mds->mds[i].tid == cur_tid) {
            Debug("Existing tid: %zu->%zu\n", cur_tid, i);
            thread_id = i;
            tid = cur_tid;
            return;
        }
    }

//    Debug("start init with thread id: %d\n", thread_bump);
    thread_id = atomic_add32(thread_bump, 1);
    thread_mds->mds[thread_id].tid = cur_tid;
    Debug("start init with thread id: %zu bump: %d\n", thread_id, *thread_bump);

    size_t i;
    for (i = 0; i < SMALL_BAG_N; i++) {
        if (thread_mds->mds[thread_id].small_bags[i].initialized != 0) {
            thread_mds->mds[thread_id].small_bags[i].initialized = 0;
        }
        thread_mds->mds[thread_id].small_bags[i].block_storage_size = SMALL_GRANULARITY * (i + 1);
        thread_mds->mds[thread_id].small_bags[i].block_data_size = SMALL_DATA_GRANULARITY * (i + 1);
    }

    for (i = 0; i < MEDIUM_BAG_N; i++) {
        if (thread_mds->mds[thread_id].medium_bags[i].initialized != 0) {
            thread_mds->mds[thread_id].medium_bags[i].initialized = 0;
        }
        thread_mds->mds[thread_id].medium_bags[i].block_storage_size = MEDIUM_GRANULARITY * (i + 1) + SMALL_THRESHOLD;
        thread_mds->mds[thread_id].medium_bags[i].block_data_size = MEDIUM_DATA_GRANULARITY * (i + 1) + SMALL_DATA_THRESHOLD;
    }

    for (i = 0; i < LARGE_BAG_N; i++) {
        if (thread_mds->mds[thread_id].large_bags[i].initialized != 0) {
            thread_mds->mds[thread_id].large_bags[i].initialized = 0;
        }
        thread_mds->mds[thread_id].large_bags[i].block_storage_size = LARGE_GRANULARITY * (i + 1) + MEDIUM_THRESHOLD;
        thread_mds->mds[thread_id].large_bags[i].block_data_size = LARGE_DATA_GRANULARITY * (i + 1) + MEDIUM_DATA_THRESHOLD;
    }

    thread_mds->mds[thread_id].huge_bag = NULL;
    thread_mds->mds[thread_id].deferred_free_list = NULL;

    allocate_tlb(&thread_mds->mds[thread_id]);

    thread_mds->mds[thread_id].pcg_seed.inc = time(NULL);
    thread_mds->mds[thread_id].pcg_seed.state = time(NULL);

    pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed);

    uint32_t tmp;
    unsigned char aes_seed[AES128_BYTE];
    for (i = 0; i < 16 / sizeof(uint32_t); i++) {
        tmp = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed);

        aes_seed[i * sizeof(uint32_t) + 0] = (tmp >> 24) & 0xff;
        aes_seed[i * sizeof(uint32_t) + 1] = (tmp >> 16) & 0xff;
        aes_seed[i * sizeof(uint32_t) + 2] = (tmp >>  8) & 0xff;
        aes_seed[i * sizeof(uint32_t) + 3] =  tmp        & 0xff;
    }
    generate_subkey(aes_seed, thread_mds->mds[thread_id].K1, thread_mds->mds[thread_id].K2);

    thread_mds->mds[thread_id].metadata_pool_ptr = create_metadata_pool();
}

#ifdef LOG_TO_FILE
#include <fcntl.h>
#include <unistd.h>
#endif

static inline void init_process() {

#ifdef LOG_TO_FILE
    int fd_logfile = open("logfile.txt", O_RDWR | O_CREAT | O_APPEND, 0644);
    if( 0 <= fd_logfile ){
        dup2(fd_logfile, STDERR_FILENO);
        close(fd_logfile);
    }
#endif

    if (thread_mds == 0) {
        pthread_mutex_lock(&thread_init_lock);
        if (thread_mds == 0) {
            Debug("Init process with tid=%zu\n", get_thread_id());
            thread_mds = mmap(NULL, sizeof(fl_metadata_all_t), PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANON, -1, 0);

            thread_bump = mmap(NULL, sizeof(atomic32_t), PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANON, -1, 0);
            *thread_bump = 0;

            pthread_mutex_unlock(&thread_init_lock);
        }
    }
    init_thread();

    Debug("%d: finish init\n", 0);
}

void print_summary() {
#ifdef ENABLE_STAT
    size_t smallSize = 0;
    size_t mediumSize = 0;
    size_t largeSize = 0;
    size_t hugeSize = 0;

    size_t smallEmpty = 0;
    size_t mediumEmpty = 0;
    size_t largeEmpty = 0;

    size_t freeBlock = 0;

    size_t smallCount[SMALL_BAG_N];
    for (size_t i = 0; i < SMALL_BAG_N; i++) {
        smallCount[i] = 0;
    }

    for (size_t i = 0; i <= *thread_bump; i++) {
        free_block_t* freeHead = thread_mds->mds[i].deferred_free_list;
        while (freeHead) {
            freeBlock += 1;
            freeHead = freeHead->nxt;
        }

        for (size_t j = 0; j < SMALL_BAG_N; j++) {
            smallSize +=
                    thread_mds->mds[i].small_bags[j].global_taken_count;
            smallEmpty += thread_mds->mds[i].small_bags[j].global_free_count;
            smallCount[i] += thread_mds->mds[i].small_bags[j].global_taken_count;
        }

        for (size_t j = 0; j < MEDIUM_BAG_N; j++) {
            mediumSize +=
                    thread_mds->mds[i].medium_bags[j].global_taken_count;
            mediumEmpty += thread_mds->mds[i].medium_bags[j].global_free_count;
        }

        for (size_t j = 0; j < LARGE_BAG_N; j++) {
            largeSize +=
                    thread_mds->mds[i].large_bags[j].global_taken_count;
            largeEmpty += thread_mds->mds[i].large_bags[j].global_free_count;
        }

        fl_huge_bag_t* head = thread_mds->mds[i].huge_bag;
        while (head) {
//            fprintf(stderr, "%p\n", head);
            hugeSize += head->storage_size;
            head = head->nxt;
        }
    }
    fprintf(stderr, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    fprintf(stderr, "%d\n", *thread_bump);
    fprintf(stderr, "small %zu\n", smallSize);
    fprintf(stderr, "medium %zu\n", mediumSize);
    fprintf(stderr, "large %zu\n", largeSize);
    fprintf(stderr, "huge %zu\n", hugeSize);
    fprintf(stderr, "total %zu\n", smallSize + mediumSize + largeSize + hugeSize);

    fprintf(stderr, "######################################\n");
    fprintf(stderr, "small %zu\n", smallEmpty);
    fprintf(stderr, "medium %zu\n", mediumEmpty);
    fprintf(stderr, "large %zu\n", largeEmpty);

    for (size_t i = 0; i < 10; i++) {
        fprintf(stderr, "%zu: %zu\n", i, smallCount[i]);
    }

    fprintf(stderr, "free: %zu\n", freeBlock);
#endif
}


#endif //UNIALLOC_THREADS_H
