#include <stdio.h>
#include "flmalloc.h"
char* ptr;
char* ptr2;
char* ptr3;

int main() {
    ptr = fl_malloc(262144);
    for (int i = 0; i < 262144; i++) {
        ptr[i] = 1;
    }

    ptr2 = fl_malloc(262144);
    ptr3 = fl_malloc(262144);
    for (int i = 0; i < 262144; i++) {
        ptr2[i] = 1;
    }
    fl_free(ptr2);


    for (int i = 0; i < 262144; i++) {
        ptr[i] = 1;
    }
    fl_free(ptr);
    fl_free(ptr3);
    return 0;
}