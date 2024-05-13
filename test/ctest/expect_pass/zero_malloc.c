#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    ptr = fl_malloc(0);
    return 0;
}`