#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(1024);
    fl_free(ptr);
    return 0;
}