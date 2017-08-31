#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Check for boundary write errors off the end of an allocated block.

int main() {
    int* ptr = (int*) malloc(sizeof(int) * 10);
    for (int i = 0; i <= 10 /* Whoops! Should be < */; ++i) {
        ptr[i] = i;
    }
    free(ptr);
    m61_printstatistics();
}

//! MEMORY BUG???: detected wild write during free of pointer ???
//! ???
