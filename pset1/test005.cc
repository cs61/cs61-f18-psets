#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
// Failed allocation.

int main() {
    void* ptrs[10];
    for (int i = 0; i < 10; ++i) {
        ptrs[i] = malloc(i + 1);
    }
    for (int i = 0; i < 5; ++i) {
        free(ptrs[i]);
    }
    size_t very_large_size = (size_t) -1 - 150;
    void* garbage = malloc(very_large_size);
    assert(!garbage);
    m61_printstatistics();
}

// The text within ??{...}?? pairs is a REGULAR EXPRESSION.
// (Some sites about regular expressions:
//  http://www.lornajane.net/posts/2011/simple-regular-expressions-by-example
//  https://www.icewarp.com/support/online_help/203030104.htm
//  http://xkcd.com/208/
// Dig deeper into how regular expresisons are implemented:
//  http://swtch.com/~rsc/regexp/regexp1.html )
// This particular regular expression lets our check work correctly on both
// 32-bit and 64-bit architectures. It checks for a `fail_size` of either
// 2^32 - 151 or 2^64 - 151.

//! ???
//! alloc count: active          5   total         10   fail          1
//! alloc size:  active        ???   total         55   fail ??{4294967145|18446744073709551465}??
