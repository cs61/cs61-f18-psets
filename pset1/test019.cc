#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Wild free.

int main() {
    int x;
    free(&x);
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not in heap
//! ???
