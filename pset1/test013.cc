#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Calloc.

const char data[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int main() {
    char* p = (char*) calloc(10, 1);
    assert(p != nullptr);
    assert(memcmp(data, p, 10) == 0);
    m61_printstatistics();
}

//! alloc count: active          1   total          1   fail          0
//! alloc size:  active         10   total         10   fail          0
