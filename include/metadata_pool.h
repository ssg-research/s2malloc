#ifndef METADATA_POOL_H
#define METADATA_POOL_H

#include "defines.h"
#include "atomic.h"
#include "debug.h"


static inline metadata_pool_t* create_metadata_pool();
static inline void destroy_metadata_pool(metadata_pool_t* pool);

static inline fl_huge_bag_t* allocate_huge_bag(metadata_pool_t* pool);
static inline void free_huge_bag(metadata_pool_t* pool, fl_huge_bag_t* ptr);

static inline free_block_t* allocate_defer_free(metadata_pool_t* pool);
static inline void free_defer_free(metadata_pool_t* pool, free_block_t* ptr); // 2. add functions

static inline fl_bag_t * allocate_regular_bag(metadata_pool_t* pool);
static inline void free_regular_bag(metadata_pool_t* pool, fl_bag_t* ptr);


static inline metadata_pool_t* create_metadata_pool() {
    metadata_pool_t* pool = mmap(NULL, (METADATA_POOL_SIZE), PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON, -1, 0);
    if (UNEXPECTED(pool == 0)) {
        Error("mmap failed %p\n", pool);
        exit(-1);
    }

    pool->bump_ptr = pool + sizeof(metadata_pool_t);

    if (pool->huge_bag_list) {
        pool->huge_bag_list = NULL; // 3. init the header
    }

    if (pool->defer_free_list) {
        pool->defer_free_list = NULL;
    }

    if (pool->bag_list) {
        pool->bag_list = NULL;
    }

    return pool;
}


static inline void destroy_metadata_pool(metadata_pool_t* pool) {
    munmap(pool, METADATA_POOL_SIZE);
}


static inline fl_bag_t* allocate_regular_bag(metadata_pool_t* pool) {
    if (UNEXPECTED(pool->bag_list == NULL)) {
        if (UNEXPECTED(METADATA_POOL_SIZE -
        (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)) < sizeof(fl_bag_t) * REGULAR_BAG_ALLOC_N)) {
            Error("Exhausted metadata pool, need %lu, available %ld\n",
                  sizeof(fl_bag_t) * REGULAR_BAG_ALLOC_N,
                  METADATA_POOL_SIZE - (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)));
            exit(-1);
        }

        Debug("Allocate new metadata for regular bags at %p\n", pool->bump_ptr);
        Debug("size=(%lu), n=%lu\n", sizeof(fl_bag_t) * REGULAR_BAG_ALLOC_N, REGULAR_BAG_ALLOC_N);

        size_t i = 0;
        fl_bag_t * tmp;
        for (; i < REGULAR_BAG_ALLOC_N; i++) {
            tmp = pool->bump_ptr + i * sizeof(fl_bag_t);
            tmp->nxt = pool->bag_list;
            pool->bag_list = tmp;
        }
        pool->bump_ptr += sizeof(fl_bag_t) * REGULAR_BAG_ALLOC_N;
    }

    fl_bag_t* ptr = pool->bag_list;
    pool->bag_list = ptr->nxt;

    Debug("Allocate to %p\n", ptr);
    return ptr;
}


static inline void free_regular_bag(metadata_pool_t* pool, fl_bag_t * ptr) {
    ptr->nxt = pool->bag_list;
    pool->bag_list = ptr;
}


//4. implement functions
static inline fl_huge_bag_t* allocate_huge_bag(metadata_pool_t* pool) {
    if (UNEXPECTED(pool->huge_bag_list == NULL)) {
        if (UNEXPECTED(METADATA_POOL_SIZE -
        (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)) < HUGE_BAG_ALLOC_N * sizeof(fl_huge_bag_t))) {
            Error("Exhausted metadata pool, need %lu, available %ld\n",
                  HUGE_BAG_ALLOC_N * sizeof(fl_huge_bag_t),
                  METADATA_POOL_SIZE - (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)));
            exit(-1);
        }

        Debug("Allocate new metadata for huge bags at %p\n", pool->bump_ptr);
        Debug("size=(%lu), n=%lu\n", HUGE_BAG_ALLOC_N * sizeof(fl_huge_bag_t), HUGE_BAG_ALLOC_N);

        size_t i = 0;
        fl_huge_bag_t * tmp;
        for (; i < HUGE_BAG_ALLOC_N; i++) {
            tmp = pool->bump_ptr + i * sizeof(fl_huge_bag_t);
            tmp->nxt = pool->huge_bag_list;
            pool->huge_bag_list = tmp;
        }
        pool->bump_ptr += HUGE_BAG_ALLOC_N * sizeof(fl_huge_bag_t);
    }

    fl_huge_bag_t* ptr = pool->huge_bag_list;
    pool->huge_bag_list = ptr->nxt;

    Debug("Allocate to %p\n", ptr);
    return ptr;
}


static inline void free_huge_bag(metadata_pool_t* pool, fl_huge_bag_t* ptr) {
    ptr->nxt = pool->huge_bag_list;
    pool->huge_bag_list = ptr;
}


static inline free_block_t* allocate_defer_free(metadata_pool_t* pool) {
    if (UNEXPECTED(pool->defer_free_list == NULL)) {
        if (UNEXPECTED(METADATA_POOL_SIZE -
        (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)) < DEFER_NODE_ALLOC_N * sizeof(free_block_t))) {
            Error("Exhausted metadata pool, need %lu, available %ld\n",
                  DEFER_NODE_ALLOC_N * sizeof(free_block_t),
                  METADATA_POOL_SIZE - (pool->bump_ptr - (void*)pool - sizeof(metadata_pool_t)));
            exit(-1);
        }

        Debug("Allocate new metadata for free list at %p\n", pool->bump_ptr);
        Debug("size=(%lu), n=%lu\n", DEFER_NODE_ALLOC_N * sizeof(free_block_t), DEFER_NODE_ALLOC_N);

        size_t i = 0;
        free_block_t* tmp;
        for (; i < DEFER_NODE_ALLOC_N; i++) {
            tmp = pool->bump_ptr + i * sizeof(free_block_t);
            tmp->nxt = pool->defer_free_list;
            pool->defer_free_list = tmp;
        }
        pool->bump_ptr += DEFER_NODE_ALLOC_N * sizeof(free_block_t);
    }

    free_block_t* ptr = pool->defer_free_list;
    pool->defer_free_list = ptr->nxt;

//    Debug("Allocate to %p\n", ptr);
    return ptr;
}


static inline void free_defer_free(metadata_pool_t* pool, free_block_t* ptr) {
    ptr->nxt = pool->defer_free_list;
    pool->defer_free_list = ptr;
}


#endif //METADATA_POOL_H
