#include <stdio.h>
#include "flmalloc.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

void* ptrs[10000];

int main() {
    srand(time(NULL));
    int r;

    for (int i = 0; i < 10000; i++) {
        r = rand();
        if (i % 2) r = r % 100000;
        else r = r % (PAGE_SIZE * 10);
//        printf("allocate with size %d\n", r);
        ptrs[i] = fl_malloc(r + 1);
        memset(ptrs[i], 1, r + 1);
    }

    for (int i = 0; i < 10000; i++) {
        fl_free(ptrs[i]);
    }
    return 0;
}