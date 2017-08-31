#include "m61.hh"
#include <stdio.h>
// Total allocation counts.

int main() {
    for (int i = 0; i < 10; ++i) {
        (void) malloc(1);
    }
    m61_printstatistics();
}

// In expected output, "???" can match any number of characters.

//! alloc count: active        ???   total         10   fail        ???
//! alloc size:  active        ???   total        ???   fail        ???
