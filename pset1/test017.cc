#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Free of invalid pointer.

int main() {
    char* ptr = (char*) malloc(32);
    free(ptr - 32);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not in heap
//! ???
