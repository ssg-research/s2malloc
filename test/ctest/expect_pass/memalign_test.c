#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    for (size_t i = 1; i < 65536; i++) {
        ptr = fl_memalign(16, 16 * i);
        if ((uint64_t) ptr & 0xf) {
            fprintf(stderr, "error alignment\n");
            return -1;
        }
        fl_free(ptr);
    }

    ptr = fl_memalign((1 << 12), (1 << 12) * 3);
    if ((uint64_t) ptr & ((1 << 12) - 1)) {
        fprintf(stderr, "error alignment\n");
        return -1;
    }
    fl_free(ptr);
    return 0;
}