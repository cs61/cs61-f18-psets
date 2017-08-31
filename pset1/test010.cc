#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
// heap_min and heap_max checking, more allocations.

int main() {
    char* heap_min = nullptr;
    char* heap_max = nullptr;
    for (int i = 0; i != 100; ++i) {
        size_t sz = rand() % 100;
        char* p = (char*) malloc(sz);
        if (!heap_min || heap_min > p) {
            heap_min = p;
        }
        if (!heap_max || heap_max < p + sz) {
            heap_max = p + sz;
        }
        free(p);
    }
    m61_statistics stat;
    m61_getstatistics(&stat);
    assert(heap_min >= stat.heap_min);
    assert(heap_max <= stat.heap_max);
}
