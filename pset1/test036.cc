#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <vector>
// Demonstrate destructors.

void f() {
    std::vector<int> v;
    for (int i = 0; i != 100; ++i) {
        v.push_back(i);
    }
    // `v` has automatic lifetime, so it is destroyed here.
}

int main() {
    f();
    m61_printstatistics();
}

//! alloc count: active          0   total    ??>=1??   fail          0
//! alloc size:  active          0   total  ??>=400??   fail          0
