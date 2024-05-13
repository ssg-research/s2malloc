#include <stdio.h>
#include "flmalloc.h"

void* ptrs[100];
extern fl_metadata_t  md;

int main() {
    for (int i = 0; i < 100; i++) {
        if (i % 2) {
            ptrs[i] = fl_malloc(LARGE_THRESHOLD + 1000);
        } else {
            ptrs[i] = fl_malloc(LARGE_THRESHOLD + 1000 + PAGE_SIZE);
        }
    }

    for (int i = 0; i < 50; i++) {
        fl_free(ptrs[i]);
    }

    for (int i = 0; i < 50; i++) {
        ptrs[i] = fl_malloc(LARGE_THRESHOLD + 1000 + 2 * PAGE_SIZE);
    }

    for (int i = 0; i < 100; i++) {
        fl_free(ptrs[i]);
    }

    return 0;
}