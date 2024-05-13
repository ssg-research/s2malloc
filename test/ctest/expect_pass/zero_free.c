#include <stdio.h>
#include "flmalloc.h"
void* ptr;

int main() {
    fl_free(0);
    return 0;
}