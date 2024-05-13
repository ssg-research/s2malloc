#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(16);
    *(((unsigned char*)ptr) + 35) += 1;
    fl_free(ptr);
    return 0;
}