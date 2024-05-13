#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(1000);
    fl_free(ptr + 1);
    return 0;
}