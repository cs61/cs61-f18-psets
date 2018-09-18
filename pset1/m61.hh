#ifndef M61_H
#define M61_H 1
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <new>


/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
void* m61_malloc(size_t sz, const char* file, long line);

/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`.
void m61_free(void* ptr, const char* file, long line);

/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. The memory
///    should be initialized to zero.
void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line);

/// m61_statistics
///    Structure tracking memory statistics.
struct m61_statistics {
    unsigned long long nactive;         // # active allocations
    unsigned long long active_size;     // # bytes in active allocations
    unsigned long long ntotal;          // # total allocations
    unsigned long long total_size;      // # bytes in total allocations
    unsigned long long nfail;           // # failed allocation attempts
    unsigned long long fail_size;       // # bytes in failed alloc attempts
    char* heap_min;                     // smallest allocated addr
    char* heap_max;                     // largest allocated addr
};

/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.
void m61_getstatistics(m61_statistics* stats);

/// m61_printstatistics()
///    Print the current memory statistics.
void m61_printstatistics();

/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.
void m61_printleakreport();


/// `m61.cc` should use these functions rather than malloc() and free().
void* base_malloc(size_t sz);
void base_free(void* ptr);
void base_allocate_disable(int is_disabled);


/// Override system versions with our versions.
#if !M61_DISABLE
#define malloc(sz)          m61_malloc((sz), __FILE__, __LINE__)
#define free(ptr)           m61_free((ptr), __FILE__, __LINE__)
#define calloc(nmemb, sz)   m61_calloc((nmemb), (sz), __FILE__, __LINE__)
#endif


/// Default locations for C++ allocators.
extern thread_local const char* m61_file;
extern thread_local int m61_line;
#define m61_here            ({ m61_file = __FILE__; m61_line = __LINE__; })
#define m61_set_location(file, line) ({ m61_file = (file); m61_line = (line); })


/// This magic class lets standard C++ containers use the system allocator,
/// instead of the debugging allocator.
template <typename T>
class system_allocator {
public:
    typedef T value_type;
    system_allocator() noexcept = default;
    template <typename U> system_allocator(system_allocator<U>&) noexcept {}

    T* allocate(size_t n) {
        return reinterpret_cast<T*>((malloc)(n * sizeof(T)));
    }
    void deallocate(T* ptr, size_t) {
        (free)(ptr);
    }
};
template <typename T, typename U>
inline constexpr bool operator==(const system_allocator<T>&, const system_allocator<U>&) {
    return true;
}
template <typename T, typename U>
inline constexpr bool operator!=(const system_allocator<T>&, const system_allocator<U>&) {
    return false;
}

#endif
