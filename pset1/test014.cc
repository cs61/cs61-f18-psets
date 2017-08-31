#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Diabolical calloc.

int main() {
    size_t very_large_nmemb = (size_t) -1 / 8 + 2;
    void* p = calloc(very_large_nmemb, 16);
    assert(p == nullptr);
    m61_printstatistics();
}

//! alloc count: active          0   total          0   fail          1
//! alloc size:  active          0   total          0   fail        ???
