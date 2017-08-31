#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
// heap_min and heap_max checking, no overlap with other regions.

static int global;

int main() {
    for (int i = 0; i != 100; ++i) {
        size_t sz = rand() % 100;
        char* p = (char*) malloc(sz);
        free(p);
    }
    m61_statistics stat;
    m61_getstatistics(&stat);

    union {
        char* cptr;
        int* iptr;
        m61_statistics* statptr;
        int (*mainptr)();
    } x;
    x.iptr = &global;
    assert(x.cptr + sizeof(int) < stat.heap_min || x.cptr >= stat.heap_max);
    x.statptr = &stat;
    assert(x.cptr + sizeof(int) < stat.heap_min || x.cptr >= stat.heap_max);
    x.mainptr = &main;
    assert(x.cptr + sizeof(int) < stat.heap_min || x.cptr >= stat.heap_max);
}
