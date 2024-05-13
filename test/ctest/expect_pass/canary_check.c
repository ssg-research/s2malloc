#include <stdio.h>
#include "flmalloc.h"
void* ptr;
char strs[] = "1234567890123456789";

int main() {
    for (int k = 0; k < 1000; k++) {
        ptr = fl_malloc(19);
        for (int i = 0; i < 19; i++) {
            *(char*)(ptr + i) = strs[i];
        }
        fl_free(ptr);
    }
    return 0;
}