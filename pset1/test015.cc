#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Another nasty calloc.

struct large_struct {
    short a[0x80000001UL];
};

int main() {
    void* p = calloc(0x100000001UL, sizeof(large_struct));
    assert(p == nullptr);
    m61_printstatistics();
}

//! alloc count: active          0   total          0   fail          1
//! alloc size:  active          0   total          0   fail        ???
