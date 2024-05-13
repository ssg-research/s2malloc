#ifndef UNIALLOC_HELPERS_H
#define UNIALLOC_HELPERS_H

#include "defines.h"


// https://stackoverflow.com/questions/8871204/count-number-of-1s-in-binary-representation
inline static size_t bit_count(size_t value) {
    size_t count;

    count = value - ((value >> 1) & 033333333333) - ((value >> 2) & 011111111111);
    return ((count + (count >> 3)) & 030707070707) % 63;
}


// https://stackoverflow.com/questions/6938219
inline static size_t memvcmp (void* memory, size_t size) {
    unsigned char* mm = (unsigned char*) memory;
    return (*mm == 0) && memcmp(mm, mm + 1, size - 1) == 0;
}
#endif //UNIALLOC_HELPERS_H
