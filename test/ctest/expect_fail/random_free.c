#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(1000);
    fl_free((void *) ((size_t) ptr * 3 + 100));
    return 0;
}