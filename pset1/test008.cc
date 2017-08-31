#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Null pointers are freeable.

int main() {
    void* p = malloc(10);
    free(nullptr);
    free(p);
    m61_printstatistics();
}

//! alloc count: active          0   total          1   fail          0
//! alloc size:  active          0   total         10   fail          0
