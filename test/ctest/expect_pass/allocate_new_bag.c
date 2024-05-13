#include <stdio.h>
#include "flmalloc.h"
void* ptrs[1000];

int main() {
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = fl_malloc(16);
    }

    for (int i = 0; i < 500; i++) {
        fl_free(ptrs[i]);
    }

    for (int i = 0; i < 500; i++) {
        ptrs[i] = fl_malloc(16);
    }

    for (int i = 0; i < 1000; i++) {
        fl_free(ptrs[i]);
    }
    return 0;
}