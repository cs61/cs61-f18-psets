#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Advanced error message for freeing data inside another heap block.

int main() {
    void* ptr = malloc(2001);
    free((char*) ptr + 128);
    m61_printstatistics();
}

//! MEMORY BUG: test???.cc:9: invalid free of pointer ???, not allocated
//!   test???.cc:8: ??? is 128 bytes inside a 2001 byte region allocated here
//! ???
