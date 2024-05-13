#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    for (int i = 0; i < 1000; i++) {
        ptr = fl_malloc(LARGE_THRESHOLD + i * 1000);
        fl_free(ptr);
    }
    return 0;
}