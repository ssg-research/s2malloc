#ifndef UNIALLOC_DEFINES_H
#define UNIALLOC_DEFINES_H

/// global definitions

#define MAX_THREAD 128

#define PAGE_SIZE_BIT 12
#define PAGE_SIZE (1 << PAGE_SIZE_BIT)

#define SMALL_BAG_N 64 // 1024 [16, 1024)
#define MEDIUM_BAG_N 14 // 8k  [1k, 8k)
#define LARGE_BAG_N 14 // [8k, 64k)

#define SMALL_GRANULARITY 16
#define MEDIUM_GRANULARITY 512
#define LARGE_GRANULARITY PAGE_SIZE

#define HUGE_BAG_ALLOC_N 256
#define DEFER_NODE_ALLOC_N 256
#define REGULAR_BAG_ALLOC_N 256

/// entropy introduced per bag
#define SMALL_ENTROPY 4
#define MEDIUM_ENTROPY 128
#define LARGE_ENTROPY (1 << 10)

#define INITIAL_MMAP_SIZE (32UL << 30)

#define SMALL_THRESHOLD (SMALL_BAG_N * SMALL_GRANULARITY) // 1k
#define MEDIUM_THRESHOLD (MEDIUM_BAG_N * MEDIUM_GRANULARITY + SMALL_THRESHOLD) // 8k
#define LARGE_THRESHOLD (LARGE_BAG_N * LARGE_GRANULARITY + MEDIUM_THRESHOLD) // 64k

#define METADATA_POOL_SIZE (1UL << 30)

#define BAG_SIZE (PAGE_SIZE / SMALL_GRANULARITY)

#define HUGE_ENTROPY 0.0

#define NULLPTR 0x00
#define INVALID_INDEX (1 << 12)
#define AES128_BYTE 16
#define ADDR_BYTE 8


//#define ENABLE_STAT
#define RELEASE_MEMORY_NAIVE
//#define ASSERT

/// regular guard page
//#define USE_GUARD_PAGE

/// random guard page
#define USE_RANDOM_GUARD_PAGE
#define GUARD_PAGE_RATE 0.1

/// empty canary
//#define CANARY_PER_EMPTY_BLOCK 4
#define EMPTY_CANARY_NEAR_BLOCK_CHECK_RANGE 2
#define EMPTY_CANARY_BYTE_LENGTH 8
#define PCG_BYTE_LENGTH (32 / CHAR_BIT)
#define EMPTY_CANARY_PCG_LENGTH EMPTY_CANARY_BYTE_LENGTH / PCG_BYTE_LENGTH

/// use canary or not
#define USE_CANARY

#ifdef USE_CANARY
#define CANARY_STORE_OVERHEAD 1
#else
#define CANARY_STORE_OVERHEAD 0
#endif

//#define BREAK_ON_FREE

#define SMALL_DATA_GRANULARITY (SMALL_GRANULARITY - SMALL_ENTROPY)
#define MEDIUM_DATA_GRANULARITY (MEDIUM_GRANULARITY - MEDIUM_ENTROPY)
#define LARGE_DATA_GRANULARITY (LARGE_GRANULARITY - LARGE_ENTROPY)

/// offset threshold
#define SMALL_DATA_THRESHOLD (SMALL_BAG_N * SMALL_DATA_GRANULARITY) // 768
#define MEDIUM_DATA_THRESHOLD (MEDIUM_BAG_N * MEDIUM_DATA_GRANULARITY + SMALL_DATA_THRESHOLD) // 5376 + 768 = 6144
#define LARGE_DATA_THRESHOLD (LARGE_BAG_N * LARGE_DATA_GRANULARITY + MEDIUM_DATA_THRESHOLD) // 43008 + 6144 = 49152

#define RANDOM_ALLOCATION_ENTROPY (1 << 8)

/// optional definitions
//#define WORD_ALIGN
//#define DEBUG

#define MIN_ALIGN 16

#ifdef DEBUG
#define WRAPPER_INFO
#endif

/// micros
#define EXPECTED(x) __builtin_expect((x), 1)
#define UNEXPECTED(x) __builtin_expect((x), 0)

#include <stddef.h>
#include <limits.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>


/// random
typedef struct {
    uint64_t state;  uint64_t inc;
} pcg32_random_t;


typedef struct free_block_t {
    struct free_block_t* nxt;
    void* val;
    size_t offset;
}free_block_t;

//! struct for each bag, note that we do not need to store the location of the canaries
typedef struct fl_bag_t {
    struct fl_bag_t* nxt;
    size_t offset_table[BAG_SIZE];
    unsigned char bitmap[BAG_SIZE / CHAR_BIT];
    void* bag;
} fl_bag_t;

//! struct for the first bag of each bucket
typedef struct fl_metadata_bag_t {
    fl_bag_t* nxt;
    size_t initialized;
    size_t block_storage_size;
    size_t block_data_size;
    void* allocation_buffer[RANDOM_ALLOCATION_ENTROPY];
    free_block_t* allocation_freelist;
#ifdef ENABLE_STAT
    size_t global_taken_count;
    size_t number_of_bags;
#endif
} fl_metadata_bag_t;


//! struct for each huge block -- this is indeed not a bag
typedef struct fl_huge_bag_t {
    struct fl_huge_bag_t * nxt;
    size_t size;
    size_t storage_size;
    void* addr;
} fl_huge_bag_t;


//! struct for the memory addr - bag head translation
typedef struct fl_tlb_t {
    struct fl_tlb_t* nxt;
    void* page_map_to_bag[INITIAL_MMAP_SIZE / PAGE_SIZE];
    void* page_map_to_head[INITIAL_MMAP_SIZE / PAGE_SIZE];
    void* pool_base_ptr;
    void* pool_bump;
} fl_tlb_t;

//! struct for the metadata pool
typedef struct metadata_pool_t { // 1. update the struct
    fl_huge_bag_t* huge_bag_list;
    free_block_t* defer_free_list;
    fl_bag_t* bag_list;
    void* bump_ptr;
} metadata_pool_t;

//! struct for the global metadata, used only once
typedef struct fl_metadata_t {
    fl_metadata_bag_t small_bags[SMALL_BAG_N];
    fl_metadata_bag_t medium_bags[MEDIUM_BAG_N];
    fl_metadata_bag_t large_bags[LARGE_BAG_N];
    fl_huge_bag_t* huge_bag;
    free_block_t* deferred_free_list;
    size_t tid;
    fl_tlb_t* tlb;
    pcg32_random_t pcg_seed;
    metadata_pool_t* metadata_pool_ptr;

    unsigned char K1[AES128_BYTE];
    unsigned char K2[AES128_BYTE];
} fl_metadata_t;

typedef struct fl_metadata_all_t {
    fl_metadata_t mds[MAX_THREAD];
} fl_metadata_all_t;

#endif //UNIALLOC_DEFINES_H
