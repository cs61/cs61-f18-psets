#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// heap_min and heap_max checking, simple case.

int main() {
    char* p = (char*) malloc(10);
    m61_statistics stat;
    m61_getstatistics(&stat);
    assert(p >= stat.heap_min);
    assert(p + 10 <= stat.heap_max);
}
