#include <stdio.h>
#include "flmalloc.h"
#include "string.h"
void* ptr;

int main() {
    ptr = fl_malloc(16);
    strcpy(ptr, "1234567890");
    ptr = fl_realloc(ptr, 32);
    strcat(ptr, "1234567890");

    if (strcmp(ptr, "12345678901234567890") != 0) {
        fprintf(stderr, "got %s\n", (char*)ptr);
        exit(-1);
    }

    fl_free(ptr);
    return 0;
}