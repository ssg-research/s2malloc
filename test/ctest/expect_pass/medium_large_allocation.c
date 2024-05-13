#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(7065);
    fl_free(ptr);
    ptr = fl_malloc(PAGE_SIZE * 3);
    fl_free(ptr);
    ptr = fl_malloc(PAGE_SIZE / 2);
    fl_free(ptr);
    ptr = fl_malloc(57477);
    fl_free(ptr);
    ptr = fl_malloc(61179);
    fl_free(ptr);
    return 0;
}