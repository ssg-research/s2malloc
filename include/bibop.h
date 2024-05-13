
#ifndef UNIALLOC_BIBOP_H
#define UNIALLOC_BIBOP_H

#include "defines.h"
#include "helpers.h"
#include "tlb.h"


static inline void add_to_freelist(fl_metadata_bag_t* bag, free_block_t* node) {
    node->nxt = bag->allocation_freelist;
    bag->allocation_freelist = node;
    Debug("Add %p\n", node->val);
}


static inline free_block_t* get_freelist_head(fl_metadata_bag_t* bag) {
    free_block_t* ptr = bag->allocation_freelist;
    if (UNEXPECTED(ptr == NULL)) {
        Debug("empty free list for size %zu\n", bag->block_storage_size);
        return NULL;
    }
    bag->allocation_freelist = ptr->nxt;
    return ptr;
}


static inline fl_metadata_bag_t* size2bag(size_t size) {
    // small
    if (EXPECTED(size <= SMALL_DATA_THRESHOLD)) {
        size_t bag_count = size / SMALL_DATA_GRANULARITY;
        Debug("small allocation to bag %zu with data size %zu\n",
              bag_count, thread_mds->mds[thread_id].small_bags[bag_count].block_data_size);

        return &thread_mds->mds[thread_id].small_bags[bag_count];

        // medium
    } else if (size <= MEDIUM_DATA_THRESHOLD) {
        size_t bag_count = (size - SMALL_DATA_THRESHOLD) / MEDIUM_DATA_GRANULARITY;
        Debug("medium allocation to bag %zu with data size %zu\n",
              bag_count, thread_mds->mds[thread_id].medium_bags[bag_count].block_data_size);

        return &thread_mds->mds[thread_id].medium_bags[bag_count];

        // large
    } else {
        size_t bag_count = (size - MEDIUM_DATA_THRESHOLD) / LARGE_DATA_GRANULARITY;
        Debug("large allocation to bag %zu with data size %zu\n",
              bag_count, thread_mds->mds[thread_id].large_bags[bag_count].block_data_size);

        return &thread_mds->mds[thread_id].large_bags[bag_count];
    }
}


static inline void map_new_pages_to_bag(fl_metadata_bag_t* head) {
    size_t size = head->block_storage_size * BAG_SIZE;
    Debug("map a new page size: %zu\n", size);

    fl_tlb_t* tlb = acquire_tlb(&thread_mds->mds[thread_id], size);

    // get a new bag
    fl_bag_t* current_bag = allocate_regular_bag(thread_mds->mds[thread_id].metadata_pool_ptr);

    // update the bag
    current_bag->nxt = head->nxt;
    head->nxt = current_bag;
    current_bag->bag = tlb->pool_bump;
    Debug("New bag: %p thread: %zu\n", current_bag->bag, thread_id);

    // update the free count and the bitmap
    size_t page_count = size >> PAGE_SIZE_BIT;
#ifdef USE_RANDOM_GUARD_PAGE
    uint32_t rand_number = pcg32_random_r(&thread_mds->mds[thread_id].pcg_seed);
    size_t guard_block_start, guard_block_end;

    if (UNEXPECTED((rand_number & 0xff) < 25)) {
        // randomly eat one page
        size_t protected_page_id = rand_number % page_count;
        guard_block_start = (protected_page_id) * PAGE_SIZE / head->block_storage_size; // included
        guard_block_end = (protected_page_id + 1) * PAGE_SIZE / head->block_storage_size; // included
        if (UNEXPECTED(guard_block_end == UINT32_MAX - 1)) {
            guard_block_end = 0;
        }

        if (UNEXPECTED((protected_page_id + 1) * PAGE_SIZE % head->block_storage_size == 0)) {
            guard_block_end--;
        }

        Assert(guard_block_start < 1000, "oob start guard");
        Assert(guard_block_end < 1000, "oob end guard");
        Assert(guard_block_end >= guard_block_start, "end should be larger than start");

        Debug("Create a random guard page at page %zu\n", protected_page_id);
        Debug("Guard blocks from %zu to %zu\n", guard_block_start, guard_block_end);
        Debug("Guard %zu blocks\n", guard_block_end - guard_block_start + 1);

        // protect the page
        Debug("Protection start :%p\n", tlb->pool_bump + PAGE_SIZE * protected_page_id);
        if (~mprotect(tlb->pool_bump + PAGE_SIZE * protected_page_id, PAGE_SIZE - 1, PROT_NONE) == 0) {
            Error("cannot set guard page at %p\n", tlb->pool_bump + PAGE_SIZE * protected_page_id);
            perror("mprotect");
            exit(-1);
        }
    } else {
        Debug("No random guard page, count+%d\n", BAG_SIZE);

        guard_block_end = SIZE_MAX;
        guard_block_start = SIZE_MAX;
    }
#else
    head->global_free_count += BAG_SIZE;
    current_bag->free_count = BAG_SIZE;

    // update the bitmap
    for (size_t i = 0; i < BAG_SIZE / CHAR_BIT; i++) {
        current_bag->bitmap[i] = 0;
    }
#endif

    // update the map
    Debug("update the map number: %zu\n", page_count);
    size_t start_page_id = (tlb->pool_bump - tlb->pool_base_ptr) >> PAGE_SIZE_BIT;
    Debug("update start at: %zu\n", start_page_id);

    for (size_t current_page = start_page_id; current_page < start_page_id + page_count; current_page++) {
        tlb->page_map_to_bag[current_page] = current_bag;
        tlb->page_map_to_head[current_page] = head;
    }

    // update the bump pointer
    tlb->pool_bump += size;
    Debug("update the bump %p, thread_id=%zu\n", tlb->pool_bump, thread_id);

#ifdef USE_CANARY
    // allocate all free bags with the largest size_t, use all 0s as canary
    for (size_t block_id = 0; block_id < BAG_SIZE; block_id++) {
#ifdef USE_RANDOM_GUARD_PAGE
        if (block_id <= guard_block_end && block_id >= guard_block_start) {
//            Debug("Block %zu guard\n", block_id);
            continue;
        }
#endif

        current_bag->offset_table[block_id] = SIZE_MAX;
        free_block_t* node = allocate_defer_free(thread_mds->mds[thread_id].metadata_pool_ptr);
        node->val = current_bag->bag + head->block_storage_size * block_id;
        node->offset= SIZE_MAX;
        add_to_freelist(head, node);
    }
#endif

#ifdef USE_GUARD_PAGE
    if (UNEXPECTED(
            INITIAL_MMAP_SIZE - (tlb->pool_bump - tlb->pool_base_ptr) < PAGE_SIZE)) {
        Error("No enough space: need: %d, available %zu\n",
              PAGE_SIZE,
              (size_t)(tlb->pool_bump - tlb->pool_base_ptr));
        exit(-1);
    }

    if (UNEXPECTED(~mprotect(tlb->pool_bump, PAGE_SIZE-1, PROT_NONE) == 0)) {
        Error("cannot set guard page at %p, thread_id=%zu\n", tlb->pool_bump, thread_id);
        Error("Base: %p, Bump: %p\n", tlb->pool_base_ptr, tlb->pool_bump);
        perror("mprotect");
        exit(-1);
    }

    tlb->pool_bump += PAGE_SIZE;
    Debug("Now, bump: %p thread: %zu\n", tlb->pool_bump, thread_id);
#endif

#ifdef ENABLE_STAT
    head->number_of_bags += 1;
#endif

    head->initialized = 1;

}

#endif //UNIALLOC_BIBOP_H
