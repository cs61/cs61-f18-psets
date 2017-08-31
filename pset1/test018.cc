#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Free of invalid pointer after some successful allocations.

int main() {
    void* ptrs[10];
    for (int i = 0; i < 10; ++i) {
        ptrs[i] = malloc(i + 1);
    }
    for (int i = 0; i < 5; ++i) {
        free(ptrs[i]);
    }
    free((void*) 16);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not in heap
//! ???
