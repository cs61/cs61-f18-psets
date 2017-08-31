#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// A correct execution should not report errors.

int main() {
    for (int i = 0; i < 10; ++i) {
        int* ptr = (int*) malloc(10 * sizeof(int));
        for (int j = 0; j < 10; ++j) {
            ptr[i] = i;
        }
        free(ptr);
    }
    m61_printstatistics();
}

//! alloc count: active          0   total         10   fail          0
//! alloc size:  active          0   total        400   fail          0
