#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(16);
    fl_free(ptr + 100);
    return 0;
}