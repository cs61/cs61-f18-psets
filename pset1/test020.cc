#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Double free.

int main() {
    void* ptr = malloc(2001);
    free(ptr);
    free(ptr);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???
//! ???
