#ifndef UNIALLOC_TLB_H
#define UNIALLOC_TLB_H
#include "defines.h"
#include "debug.h"
#include "atomic.h"


static inline void allocate_tlb(fl_metadata_t* md) {
    fl_tlb_t *ptr = mmap(NULL, sizeof(fl_tlb_t), PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (UNEXPECTED(ptr == NULL)) {
        Error("mmap failed %p\n", ptr);
        exit(-1);
    }

    ptr->pool_base_ptr = mmap(NULL, INITIAL_MMAP_SIZE, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr->pool_base_ptr == 0) {
        Error("Unable to allocate pool for thread %d\n", 1);
        exit(-1);
    }

    ptr->pool_bump = ptr->pool_base_ptr;
    md->tlb = ptr;
}


static inline void free_tlb(fl_tlb_t * ptr) {
    munmap(ptr, sizeof(fl_tlb_t));
}


static inline fl_tlb_t* ptr2tlb(fl_metadata_t* md, void* ptr) {
    fl_tlb_t* tlb = md->tlb;
    if (tlb == NULL) return NULL;
    if (ptr >= tlb->pool_base_ptr && ptr < tlb->pool_bump) {
        Debug("get tlb %p\n", tlb);
        return tlb;
    } else {
        return NULL;
    }
}


static inline fl_tlb_t* acquire_tlb(fl_metadata_t* md, size_t size) {
    return md->tlb;
}
#endif //UNIALLOC_TLB_H
