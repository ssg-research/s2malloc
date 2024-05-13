#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(LARGE_THRESHOLD + 10);
    fl_free(ptr);
    return 0;
}