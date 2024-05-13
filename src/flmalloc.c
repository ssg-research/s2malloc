#include "flmalloc.h"
#include "metadata_pool.h"
#include "threads.h"
#include "string.h"
#include "bibop.h"
#include "helpers.h"


// function declarations
inline static void* fl_malloc_huge(size_t size, size_t align);
inline static void* fl_malloc_regular(size_t size, fl_metadata_bag_t* bag, size_t align);
inline static size_t fl_free_huge(void* ptr);
inline static void fl_free_regular(void* ptr, fl_metadata_bag_t* current_head, fl_bag_t * current_bag);

inline static void empty_freelist();

// global variables
fl_metadata_all_t* thread_mds;
volatile atomic32_t* thread_bump;
__thread size_t thread_id; // index id
__thread size_t tid; // real id


void* fl_malloc(size_t size) {
    if (UNEXPECTED(tid != get_thread_id())) {
        tid = get_thread_id();
        Debug("Init new thread with tid=%zu\n", tid);
        init_process();
    }

    Debug("allocate with size %zu, thread: %zu\n", size, thread_id);
    if (UNEXPECTED(size == 0)) {
        Debug("zero size %zu\n", size);
        return NULLPTR;
    }

    size += CANARY_STORE_OVERHEAD;
    empty_freelist();

    // get bag head
    if (EXPECTED(size <= LARGE_DATA_THRESHOLD)) {
//        fl_metadata_bag_t * bag = size2bag(size);
//        Error("size: %zu, number: %zu, space: %zu\n", size, bag->global_taken_count, bag->block_storage_size);
        return fl_malloc_regular(size, size2bag(size), 1);
    } else {
        Debug("huge allocation size: %zu\n", size);
        return fl_malloc_huge(size, 1);
    }
}


inline static void* fl_malloc_huge(size_t size, size_t align) {
#ifdef WORD_ALIGN
    if (align < MIN_ALIGN) {
        align = MIN_ALIGN;
    }
#endif

    size_t new_size = (size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    Debug("new size: %zu\n", new_size);
#ifdef USE_GUARD_PAGE
    new_size += PAGE_SIZE;
#endif

    // allocate node and data block
    fl_huge_bag_t* huge_metadata_node = allocate_huge_bag(thread_mds->mds[thread_id].metadata_pool_ptr);
    void* addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON |MAP_NORESERVE, -1, 0);

    if (addr == 0) {
        Error("mmap failed %p\n", addr);
        exit(-1);
    }

#ifdef USE_GUARD_PAGE
    if (~mprotect(addr + new_size - PAGE_SIZE, PAGE_SIZE - 1, PROT_NONE) == 0) {
        Error("cannot set guard page at %p\n", addr + new_size - PAGE_SIZE);
        perror("mprotect");
        exit(-1);
    }
    Debug("Set guard page at %p\n", addr + new_size - PAGE_SIZE);
#endif

    // construct the node
    huge_metadata_node->nxt = thread_mds->mds[thread_id].huge_bag;
    huge_metadata_node->size = size;
    huge_metadata_node->storage_size = new_size;
    huge_metadata_node->addr = addr;
    // update head
    thread_mds->mds[thread_id].huge_bag = huge_metadata_node;

    // return with offset
    Debug("Huge allocation to %p\n", huge_metadata_node->addr);
    Debug("addr: %p \n", huge_metadata_node->addr);
    return huge_metadata_node->addr;
}


inline static void* fl_malloc_regular(size_t size, fl_metadata_bag_t* bag, size_t align) {
    Debug("size %zu, align %zu\n", bag->block_data_size, align);
    if (bag->initialized == 0) {
        Debug("init free bag for data size %zu\n", bag->block_data_size);
        for (size_t i = 0; i < RANDOM_ALLOCATION_ENTROPY; i++) {
           void* ptr = get_freelist_head(bag);
           while (ptr == NULL) {
               map_new_pages_to_bag(bag);
               ptr = get_freelist_head(bag);
           }

           bag->allocation_buffer[i] = ptr;
        }
        Debug("now free bag for data size %zu\n", bag->block_data_size);
    }

    // target free block
    size_t target_free_block_count = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed) & (RANDOM_ALLOCATION_ENTROPY - 1);
    free_block_t* allocated_node = bag->allocation_buffer[target_free_block_count];
    Debug("target block: %p\n", allocated_node->val);
    fl_tlb_t* tlb = ptr2tlb(&thread_mds->mds[thread_id], allocated_node->val);
    fl_bag_t* current_bag = tlb->page_map_to_bag[(allocated_node->val - tlb->pool_base_ptr) >> PAGE_SIZE_BIT];
    size_t block_index = (allocated_node->val - current_bag->bag) / bag->block_storage_size;

#ifdef USE_CANARY
    // check current and nearby blocks are not being modified after free
    size_t start_block_id;
    size_t end_block_id;

    // get the L range, included
    if (UNEXPECTED(target_free_block_count < EMPTY_CANARY_NEAR_BLOCK_CHECK_RANGE)) {
        start_block_id = 0;
    } else {
        start_block_id = target_free_block_count - EMPTY_CANARY_NEAR_BLOCK_CHECK_RANGE;
    }

    // get the R range, included
    if (UNEXPECTED(target_free_block_count >= RANDOM_ALLOCATION_ENTROPY - EMPTY_CANARY_NEAR_BLOCK_CHECK_RANGE)) {
        end_block_id = RANDOM_ALLOCATION_ENTROPY - 1;
    } else {
        end_block_id = target_free_block_count + EMPTY_CANARY_NEAR_BLOCK_CHECK_RANGE;
    }

    // check each block
    Debug("L: %zu, R: %zu\n", start_block_id, end_block_id);

    for (size_t current_block_id = start_block_id;
         current_block_id <= end_block_id;
         current_block_id++) {
        Debug("ID: %zu, buffer: %p\n", current_block_id, bag->allocation_buffer);
        free_block_t* node = bag->allocation_buffer[current_block_id];
        if (!node) {
            for (int i = 0; i < 256; i++) {
                Error("%d, %p\n", i, bag->allocation_buffer[i]);
            }
            exit(1);
        }
        Debug("Checking block %p with location %zu\n", node->val, current_block_id);

#ifndef BREAK_ON_FREE
        // free, check free canary
        if (node->offset != SIZE_MAX) {
            Debug("Not Max: %zu\n", current_block_id);
            // recently freed block
            unsigned char canary_value[EMPTY_CANARY_BYTE_LENGTH];
            hashn(thread_mds->mds[thread_id].K1,
                  thread_mds->mds[thread_id].K2,
                  (uint64_t) (node->val + node->offset),
                  canary_value);
            if (memcmp(node->val + node->offset, canary_value, EMPTY_CANARY_BYTE_LENGTH) != 0) {

                // error
                for (size_t in_block_offset = 0; in_block_offset < EMPTY_CANARY_BYTE_LENGTH; in_block_offset++) {
                    if ((unsigned)*(unsigned char*)
                            (node->val + node->offset + in_block_offset) != canary_value[in_block_offset]) {
                        Error("invalid canary: expect: %02x, get %02x\n",
                              canary_value[in_block_offset],
                              (unsigned)*(unsigned char*)(node->val + node->offset + in_block_offset));
                    }
                }

                Error("base: %p, ptr: %p, block id: %zu\n", node->val, node->val + node->offset, current_block_id);
                Error("node: %p, offset: %zu\n", node, node->offset);
                Error("block storage size: %zu\n", bag->block_storage_size);

                exit(-1);
            }

        } else { // check if current block is empty
            Debug("Max: %zu\n", current_block_id);
            if (!memvcmp(node->val, bag->block_storage_size)) {// error
                for (size_t in_block_offset = 0; in_block_offset < bag->block_storage_size; in_block_offset++) {
                    if ((unsigned)*(unsigned char*)
                            (node->val + in_block_offset) != 0) {
                        Error("invalid canary: expect: %02x, get %02x\n",
                              0,
                              (unsigned)*(unsigned char*)(node->val + in_block_offset));
                        Error("ptr: %p, block id: %zu\n", node->val, current_block_id);
                    }
                }

                exit(-1);
            }
        } // end current block canary check
#else
        if (!memvcmp(node->val, bag->block_storage_size)) {// error
            for (size_t in_block_offset = 0; in_block_offset < bag->block_storage_size; in_block_offset++) {
                if ((unsigned)*(unsigned char*)
                        (node->val + in_block_offset) != 0) {
                    Error("invalid canary: expect: %02x, get %02x\n",
                          0,
                          (unsigned)*(unsigned char*)(node->val + in_block_offset));
                    Error("ptr: %p, block id: %zu\n", node->val, current_block_id);
                }
            }

            exit(-1);
        }
#endif
    } // end for each nearby block
#endif

    // mark as use
    Debug("mark block %zu as use, bitmap: %x\n", block_index, current_bag->bitmap[block_index / CHAR_BIT]);
    current_bag->bitmap[block_index / CHAR_BIT] |= (0x1 << (block_index % CHAR_BIT));
    Debug("bitmap: %x\n", current_bag->bitmap[block_index / CHAR_BIT]);

#ifdef USE_CANARY
    size_t offset;

#ifdef WORD_ALIGN
    if (align < MIN_ALIGN) {
            align = MIN_ALIGN;
        }
#endif
    if (EXPECTED(align != 1)) {
        uint64_t base_ptr = (uint64_t) (current_bag->bag + bag->block_storage_size * block_index);
        offset = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed) % (bag->block_storage_size - bag->block_data_size - 2);
        
        uint64_t new_ptr = base_ptr + offset + align;
        new_ptr = new_ptr / align * align;
        offset = new_ptr - base_ptr;
        while (offset + bag->block_data_size >= bag->block_storage_size) {
            offset -= align;
        }
    } else {
        offset = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed) % (bag->block_storage_size - bag->block_data_size - 2);
    }

    Assert(offset + bag->block_data_size + 1 <= bag->block_storage_size, "oob offset");

    current_bag->offset_table[block_index] = offset;
    Debug("offset=%zu\n", offset);
    unsigned char canary_value;
    hash1(thread_mds->mds[thread_id].K1,
          thread_mds->mds[thread_id].K2,
          (uint64_t) (current_bag->bag + bag->block_storage_size * block_index + offset),
          &canary_value);

    // tail
    Debug("set canary %02x at %p\n",
          canary_value, current_bag->bag + bag->block_storage_size * block_index + bag->block_data_size + offset);
    *(unsigned char*)(current_bag->bag + bag->block_storage_size * block_index + bag->block_data_size + offset) = canary_value;
#else
#ifdef WORD_ALIGN
    current_bag->offset_table[block_index] = 4;
#else
    current_bag->offset_table[block_index] =
            pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed) % (bag->block_storage_size - bag->block_data_size);
#endif
#endif

    // fill into the empty cell in the buffer
    free_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr, allocated_node);
    free_block_t* new_ptr = get_freelist_head(bag);
    while (new_ptr == NULL) {
        map_new_pages_to_bag(bag);
        new_ptr = get_freelist_head(bag);
    }
    bag->allocation_buffer[target_free_block_count] = new_ptr;

    Debug("allocate with offset %zu\n", current_bag->offset_table[block_index]);
    Debug("allocate to head %p, bag %p, ptr %p (tid=%zu)\n", bag, current_bag,
          current_bag->bag + bag->block_storage_size * block_index + current_bag->offset_table[block_index], thread_id);

#ifdef ENABLE_STAT
    bag->global_taken_count += 1;
#endif
    Assert((current_bag->offset_table[block_index] + bag->block_data_size <= bag->block_storage_size),
           "offset out of bound");
    Assert((block_index < BAG_SIZE), "block index out of bound");
    return current_bag->bag + bag->block_storage_size * block_index + current_bag->offset_table[block_index];
}


inline static void fl_free_regular(void* ptr, fl_metadata_bag_t* current_head, fl_bag_t * current_bag) {
    size_t block_id = (ptr - current_bag->bag) / current_head->block_storage_size;
    Debug("free block_id : %zu\n", block_id);

    if (!(current_bag->bitmap[block_id / CHAR_BIT] & (0x1 << (block_id % CHAR_BIT)))) {
        Error("invalid pointer %p, block is not allocated\n", ptr);
        Error("expected ptr: %p\n",
              current_bag->bag + block_id * current_head->block_storage_size + current_bag->offset_table[block_id]);
        Error("Block size: %zu\n", current_head->block_storage_size);
        if (block_id <= BAG_SIZE - 2) {
            Error("next: %p\n",
                  current_bag->bag + (1 + block_id) * current_head->block_storage_size
                  + current_bag->offset_table[block_id + 1]);
            Error("next base: %p\n", current_bag->bag + (1 + block_id) * current_head->block_storage_size);
        }

        if (block_id > 0) {
            Error("prev: %p\n",
                  current_bag->bag + (-1 + block_id) * current_head->block_storage_size
                  + current_bag->offset_table[block_id - 1]);
            Error("prev base: %p\n", current_bag->bag + (-1 + block_id) * current_head->block_storage_size);
        }
        exit(-1);
    }

    if (current_bag->bag + block_id * current_head->block_storage_size + current_bag->offset_table[block_id] != ptr) {
        Error("invalid pointer offset: expected: %p, ptr: %p\n",
              current_bag->bag + block_id * current_head->block_storage_size + current_bag->offset_table[block_id],
              ptr);
        Debug("bag: %p, data size: %zu, offset: %zu\n",
              current_bag,
              current_head->block_data_size,
              current_bag->offset_table[block_id]);
        exit(-1);
    }

    // check canary
#ifdef USE_CANARY
    unsigned char canary_value;
    hash1(thread_mds->mds[thread_id].K1,
          thread_mds->mds[thread_id].K2,
          (uint64_t) ptr,
          &canary_value);

//     tail canary
     if ((unsigned)*(unsigned char*)
     (ptr + current_head->block_data_size) != canary_value) {
         Error("invalid canary: expect: %x, get %02x\n",
               canary_value,
               (unsigned)*(unsigned char*)(ptr + current_head->block_data_size));
         Error("ptr: %p\n", ptr);
         exit(-1);
     }

#ifndef BREAK_ON_FREE
     size_t offset;
     if (current_head->block_storage_size >= PAGE_SIZE) {
         // mark random bytes of the newly freed block as canary
          offset = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed)
                         % (current_head->block_storage_size - EMPTY_CANARY_BYTE_LENGTH);
         current_bag->offset_table[block_id] = offset;

         unsigned char* canary_ptr = current_bag->bag + block_id * current_head->block_storage_size + offset;
         hashn(thread_mds->mds[thread_id].K1,
               thread_mds->mds[thread_id].K2,
               (uint64_t) canary_ptr,
               canary_ptr);
         Debug("new canary location: %zu\n", offset);
     } else {
         memset(current_bag->bag + block_id * current_head->block_storage_size, 0, current_head->block_storage_size);
         offset = SIZE_MAX;
     }

#else
    memset(current_bag->bag + block_id * current_head->block_storage_size, 0, current_head->block_storage_size);
    size_t offset = SIZE_MAX;
#endif
#endif

    // mark as free
    current_bag->bitmap[block_id / CHAR_BIT] ^= (0x1 << (block_id % CHAR_BIT));


    // add back to allocation buffer
    free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
    node->val = current_bag->bag + block_id * current_head->block_storage_size;
    node->offset = offset;
    add_to_freelist(current_head, node);

#ifdef RELEASE_MEMORY_NAIVE
    if (current_head->block_storage_size >= 8 * PAGE_SIZE) {
        memset(node->val, 0, current_head->block_storage_size);
        madvise(node->val, ((current_head->block_storage_size) >> PAGE_SIZE_BIT) << PAGE_SIZE_BIT, MADV_DONTNEED);
        node->offset = SIZE_MAX;
        current_bag->offset_table[block_id] = SIZE_MAX;
    }

#endif

#ifdef ENABLE_STAT
    current_head->global_taken_count -= 1;
#endif
}


inline static void add_to_deferred_free_list(free_block_t* ptr, size_t target_thread_id) {
    while (1) {
        ptr->nxt = thread_mds->mds[target_thread_id].deferred_free_list;
        if (atomic_compare_exchange_strong(&thread_mds->mds[target_thread_id].deferred_free_list,
                                            &ptr->nxt,
                                            ptr)) {
            break;
        }
    }
}


static inline void free_object_from_another_thread(void* ptr) {
    // check regular
    size_t cur_thread_id;
    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        if (ptr2tlb(&thread_mds->mds[cur_thread_id], ptr) == NULL) {
            continue;
        }

        // find the corresponding list
        Debug("add to free list of thread: %zu, call from thread %zu\n", cur_thread_id, thread_id);
        free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
        node->val = ptr;
        add_to_deferred_free_list(node, cur_thread_id);
        return;
    }

    // check big
    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        fl_huge_bag_t* cur_huge_bag = thread_mds->mds[cur_thread_id].huge_bag;
        while (cur_huge_bag) {
            if (cur_huge_bag->addr == ptr) { // head match
                free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
                node->val = ptr;
                add_to_deferred_free_list(node, cur_thread_id);
                Debug("(large) add to free list of thread: %zu, call from thread %zu\n", cur_thread_id, thread_id);
                return;
            }

            cur_huge_bag = cur_huge_bag->nxt;
        }
    }

    // nothing, panic
    Error("Invalid ptr (unknown) %p\n", ptr);

    // debug
    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        if (ptr2tlb(&thread_mds->mds[cur_thread_id], ptr) == NULL) {
            Error("Not in thread %zu range\n", cur_thread_id);
            continue;
        }
    }

    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        fl_huge_bag_t* cur_huge_bag = thread_mds->mds[cur_thread_id].huge_bag;
        while (cur_huge_bag) {
            Error("huge ptr: %p, thread id %zu\n", cur_huge_bag, cur_thread_id);
            cur_huge_bag = cur_huge_bag->nxt;
        }
    }

    exit(-1);
}


static inline void fl_free_single(void* ptr) {
    Debug("free ptr %p\n", ptr);

    fl_tlb_t* tlb = ptr2tlb(&thread_mds->mds[thread_id], ptr);
    if (UNEXPECTED(tlb == NULL)) {
        Debug("check valid... %p (tid=%zu)\n", ptr, thread_id);
        if (EXPECTED(fl_free_huge(ptr))) {
            Debug("big pointer %p\n", ptr);
            return;
        } else {
            free_object_from_another_thread(ptr);
            Debug("add to another thread %p\n", ptr);
            return;
        }
    }

    size_t page_count = (ptr - tlb->pool_base_ptr) >> PAGE_SIZE_BIT;
    Debug("page_count %zu\n", page_count);

    fl_metadata_bag_t* current_head = tlb->page_map_to_head[page_count];
    Debug("head at %p\n", current_head);

    fl_bag_t * current_bag = tlb->page_map_to_bag[page_count];
    Debug("bag at %p\n", current_bag);

    fl_free_regular(ptr, current_head, current_bag);
}

inline static void empty_freelist() {
    // clear the free list first
    while (thread_mds->mds[thread_id].deferred_free_list) {
        Debug("Cleaning deferred free thread_id %zu ptr %p\n", thread_id, thread_mds->mds[thread_id].deferred_free_list);
        free_block_t* tmp = thread_mds->mds[thread_id].deferred_free_list;
        fl_free_single(tmp->val);
        thread_mds->mds[thread_id].deferred_free_list = tmp->nxt;
        free_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr, tmp);
    }
}


inline static size_t fl_free_huge(void* ptr) {
    fl_huge_bag_t* cur_huge_bag = thread_mds->mds[thread_id].huge_bag;
    if (!cur_huge_bag) {
        Debug("Invalid free for huge block (invalid ptr): %p, possibly another thread\n", ptr);
        return 0;
    }

    if (cur_huge_bag->addr == ptr) { // head match
        thread_mds->mds[thread_id].huge_bag = cur_huge_bag->nxt;

        munmap(cur_huge_bag->addr, cur_huge_bag->storage_size);
        free_huge_bag(thread_mds->mds[thread_id].metadata_pool_ptr, cur_huge_bag);
        return 1;
    }

    while (cur_huge_bag->nxt) {
        if (cur_huge_bag->nxt->addr != ptr) {
            cur_huge_bag = cur_huge_bag->nxt;
            continue;
        }

        // free the current block
        fl_huge_bag_t* block_to_free = cur_huge_bag->nxt;
        cur_huge_bag->nxt = cur_huge_bag->nxt->nxt;

        munmap(block_to_free->addr, block_to_free->storage_size);
        free_huge_bag(thread_mds->mds[thread_id].metadata_pool_ptr, block_to_free);
        return 1;
    }

    // no valid block, invalid free
    Debug("Invalid free for huge block (invalid ptr): %p, possibly another thread\n", ptr);
    return 0;
}


void fl_free(void* ptr) {
    if (UNEXPECTED(tid != get_thread_id())) {
        tid = get_thread_id();
        Debug("Init new thread with tid=%zu\n", tid);
        init_process();
    }

    if (UNEXPECTED(ptr == 0)) {
        Debug("Free a empty pointer: %p\n", ptr);
        return;
    }
    // clear the free list first
    empty_freelist();

    // free current
    fl_free_single(ptr);
}


static inline void realloc_from_another_thread(void* old_ptr, void* new_ptr, size_t new_size) {
    // check regular
    size_t cur_thread_id;
    fl_tlb_t * tlb;
    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        tlb = ptr2tlb(&thread_mds->mds[cur_thread_id], old_ptr);
        if (tlb == NULL) {
            Debug("not in range thread %zu\n", cur_thread_id);
            continue;
        }

        // find the corresponding list
        Debug("add to free list of thread: %zu, call from thread %zu\n", cur_thread_id, thread_id);
        size_t page_count = (uint64_t)(old_ptr - tlb->pool_base_ptr) >> PAGE_SIZE_BIT;
        Debug("page_count %zu\n", page_count);

        fl_metadata_bag_t* current_head = tlb->page_map_to_head[page_count];
        Debug("head at %p\n", current_head);

        size_t old_size = current_head->block_data_size;
        Debug("Old size: %zu\n", current_head->block_data_size);

        if (old_size < new_size) {
            memcpy(new_ptr, old_ptr, old_size);
        } else {
            memcpy(new_ptr, old_ptr, new_size);
        }
        free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
        node->val = old_ptr;
        add_to_deferred_free_list(node, cur_thread_id);
        return;
    }

    Debug("checking big %d\n", 0);
    // check big
    for (cur_thread_id = 1; cur_thread_id <= *thread_bump; cur_thread_id++) {
        fl_huge_bag_t* cur_huge_bag = thread_mds->mds[cur_thread_id].huge_bag;
        while (cur_huge_bag) {
            if (cur_huge_bag->addr == old_ptr) { // head match
                Debug("get big realloc %p\n", old_ptr);
                size_t old_size = cur_huge_bag->size;
                if (old_size < new_size) {
                    memcpy(new_ptr, old_ptr, old_size);
                } else {
                    memcpy(new_ptr, old_ptr, new_size);
                }
                Debug("done with copy %d\n", 0);

                free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
                node->val = old_ptr;
                add_to_deferred_free_list(node, cur_thread_id);
                Debug("(large) add to free list of thread: %zu, call from thread %zu\n", cur_thread_id, thread_id);
                return;
            }

            cur_huge_bag = cur_huge_bag->nxt;
        }
    }

    // nothing, panic
    Error("Invalid ptr (unknown) %p\n", old_ptr);
    fl_free(new_ptr);
    exit(-1);
}


void* fl_realloc(void* ptr, size_t size) {
//    Error("realloc with ptr: %p, size: %zu\n", ptr, size);
    if(ptr == NULL) {
        Debug("Empty ptr with size %zu\n", size);
        return fl_malloc(size);
    }

    if(size == 0) {
        Debug("Empty size with ptr %p\n", ptr);
        fl_free(ptr);
        return NULLPTR;
    }

    size_t old_size;
    fl_tlb_t* tlb = ptr2tlb(&thread_mds->mds[thread_id], ptr);
    if (tlb != NULL) {
        size_t page_count = (ptr - tlb->pool_base_ptr) >> PAGE_SIZE_BIT;
        Debug("page_count %zu\n", page_count);

        fl_metadata_bag_t* current_head = tlb->page_map_to_head[page_count];
        Debug("head at %p\n", current_head);

        old_size = current_head->block_data_size;
	//#if (size2bag(old_size) == size2bag(size)) return ptr;
        Debug("Old size: %zu\n", current_head->block_data_size);

        void* new_ptr = fl_malloc(size);
        if (old_size > size) {
            memcpy(new_ptr, ptr, size);
        } else {
            memcpy(new_ptr, ptr, old_size);
        }

        fl_free(ptr);
//        Error("Allocate to %p\n", new_ptr);
        return new_ptr;
    } else {
        Debug("not found in current thread: %zu\n", thread_id);
        void* new_ptr = fl_malloc(size);
        realloc_from_another_thread(ptr, new_ptr, size);
//        Error("Allocate to %p\n", new_ptr);
        return new_ptr;
    }
}


void* fl_calloc(size_t nmemb, size_t size) {
    Debug("Calloc with n: %zu, size %zu\n", nmemb, size);

    void* ptr = fl_malloc(size * nmemb);
    memset(ptr, 0, size * nmemb);
    return ptr;
}


void*fl_memalign(size_t alignment, size_t size) {
    if (UNEXPECTED(alignment & (alignment - 1))) {
        Error("invalid alignment: %zu\n", alignment);
        exit(-1);
    }
    Debug("alignment: %zu, size %zu\n", alignment, size);
    if (UNEXPECTED(tid != get_thread_id())) {
        tid = get_thread_id();
        Debug("Init new thread with tid=%zu\n", tid);
        init_process();
    }

    // big alignment
    if (alignment >= size) {
        size_t new_size = size + alignment * 2 + CANARY_STORE_OVERHEAD;
        if (EXPECTED(new_size <= LARGE_DATA_THRESHOLD)) {
            // reserve enough space for offset
            return fl_malloc_regular(new_size, size2bag(new_size), alignment);
        }

        return fl_malloc_huge(alignment, alignment);
    } else {
        // small alignment
        size_t new_size = size + 2 * alignment + CANARY_STORE_OVERHEAD;

        if (EXPECTED(new_size <= LARGE_DATA_THRESHOLD)) {
            return fl_malloc_regular(new_size, size2bag(new_size), alignment);
        }
        return fl_malloc_huge(new_size, alignment);
    }
}


int fl_posix_memalign(void** ptr, size_t s, size_t s2) {
    Debug("ptr: %p, alignment: %zu, size %zu\n", ptr, s, s2);
    *ptr = fl_memalign(s, s2);
    Debug("allocated to %p\n", *ptr);
    return 0;
}

void* fl_aligned_alloc(size_t s, size_t s2) {
    Debug("alignment: %zu, size %zu\n", s, s2);
    return fl_memalign(s, s2);
}
