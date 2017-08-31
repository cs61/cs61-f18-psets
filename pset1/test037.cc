#include "m61.hh"
#include <stdio.h>

char* ptrs[10];

int main(int argc, char** argv) {
    int nalloc = argc < 2 ? 10 : strtol(argv[1], nullptr, 0);
    int nfree = argc < 3 ? 5 : strtol(argv[2], nullptr, 0);
    for (int i = 0; i < nalloc; ++i) {
        ptrs[i] = new char[i + 1];
    }
    for (int i = 0; i < nfree; ++i) {
        delete[] ptrs[i];
    }
    m61_printstatistics();
}

//! alloc count: active          5   total         10   fail        ???
//! alloc size:  active   ??>=40??   total   ??>=55??   fail        ???
