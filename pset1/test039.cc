#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Wild free inside heap-allocated data.

struct whatever {
    int first[100];
    char second[3000];
    int third[200];
};

int main(int argc, char** argv) {
    whatever* object = new whatever;
    uintptr_t addr = reinterpret_cast<uintptr_t>(object);
    if (argc < 2) {
        addr += 3000;
    } else {
        addr += strtol(argv[1], nullptr, 0);
    }
    whatever* trick = reinterpret_cast<whatever*>(addr);
    delete trick;
    m61_printstatistics();
}

//! MEMORY BUG???: invalid free of pointer ???, not allocated
//! ???
