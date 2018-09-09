#define M61_DISABLE 1
#include "m61.hh"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    return base_malloc(sz);
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    base_free(ptr);
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line) {
    // Your code here (to fix test014).
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}


/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_getstatistics(m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(m61_statistics));
    // Your code here.
}


/// m61_printstatistics()
///    Print the current memory statistics.

void m61_printstatistics() {
    m61_statistics stats;
    m61_getstatistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_printleakreport() {
    // Your code here.
}


thread_local const char* m61_file = "?";
thread_local int m61_line = 1;

void* operator new(size_t sz) {
    return m61_malloc(sz, m61_file, m61_line);
}
void* operator new[](size_t sz) {
    return m61_malloc(sz, m61_file, m61_line);
}
void operator delete(void* ptr) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete(void* ptr, size_t) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete[](void* ptr) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete[](void* ptr, size_t) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
