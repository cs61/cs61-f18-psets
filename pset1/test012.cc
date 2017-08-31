#include "m61.hh"
#include <stdio.h>
#include <assert.h>
// Diabolical failed allocation.

int main() {
    void* ptrs[10];
    for (int i = 0; i < 10; ++i) {
        ptrs[i] = malloc(i + 1);
    }
    for (int i = 0; i < 5; ++i) {
        free(ptrs[i]);
    }
    size_t very_large_size = (size_t) -1;
    void* garbage = malloc(very_large_size);
    assert(!garbage);
    m61_printstatistics();
}

//! alloc count: active          5   total         10   fail          1
//! alloc size:  active         40   total         55   fail ??{4294967295|18446744073709551615}??
