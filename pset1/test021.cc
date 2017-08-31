#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Wild free inside heap-allocated data.

int main() {
    void* ptr = malloc(2001);
    free((char*) ptr + 128);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not allocated
//! ???
