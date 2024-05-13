#ifndef UNIALLOC_ATOMIC_H
#define UNIALLOC_ATOMIC_H


#include <stdatomic.h>
#include <stdlib.h>

typedef volatile _Atomic(int32_t) atomic32_t;
typedef volatile _Atomic(int64_t) atomic64_t;
typedef volatile _Atomic(void*) atomicptr_t;

static inline int32_t atomic_incr32(atomic32_t* val) { return atomic_fetch_add_explicit(val, 1, memory_order_relaxed) + 1; }
static inline int32_t atomic_decr32(atomic32_t* val) { return atomic_fetch_add_explicit(val, -1, memory_order_relaxed) - 1; }
static inline int32_t atomic_add32(atomic32_t* val, int32_t add) { return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add; }
static inline void   atomic_exchange_(atomicptr_t* dst, void* val) { atomic_exchange_explicit(dst, val, memory_order_acquire); }
#endif //UNIALLOC_ATOMIC_H
