#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <vector>
// Now C++ library functions call your allocator
// (but do not provide line number information).

int main() {
    std::vector<int> v;
    for (int i = 0; i != 100; ++i) {
        v.push_back(i);
    }
    m61_printstatistics();
}

//! alloc count: active          1   total    ??>=1??   fail          0
//! alloc size:  active  ??>=400??   total  ??>=400??   fail        ???
