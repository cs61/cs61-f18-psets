#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Free of invalid pointer.

int main() {
    free((void*) 16);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not in heap
//! ???
