#define M61_DISABLE 1
#include "m61.hh"
#include <unordered_map>
#include <vector>


// This file contains a base memory allocator guaranteed not to
// overwrite freed allocations. No need to understand it.


using base_allocation = std::pair<uintptr_t, size_t>;

// `allocs` is a hash table mapping active pointer address to allocation size.
// `frees` is a vector of freed allocations.
// Both structures are specialized to use the *system* allocator (not m61).
static std::unordered_map<uintptr_t, size_t,
            std::hash<uintptr_t>, std::equal_to<uintptr_t>,
            system_allocator<std::pair<const uintptr_t, size_t>>> allocs;
static std::vector<base_allocation, system_allocator<base_allocation>> frees;
static int disabled;

static unsigned alloc_random() {
    static uint64_t x = 8973443640547502487ULL;
    x = x * 6364136223846793005ULL + 1ULL;
    return x >> 32;
}

static void base_allocate_atexit();

void* base_malloc(size_t sz) {
    if (disabled) {
        return malloc(sz);
    }

    static int base_alloc_atexit_installed = 0;
    if (!base_alloc_atexit_installed) {
        atexit(base_allocate_atexit);
        base_alloc_atexit_installed = 1;
    }

    // try to use a previously-freed block 75% of the time
    unsigned r = alloc_random();
    if (r % 4 != 0) {
        for (unsigned ntries = 0; ntries < 10 && ntries < frees.size(); ++ntries) {
            auto& f = frees[alloc_random() % frees.size()];
            if (f.second >= sz) {
                allocs.insert(f);
                uintptr_t ptr = f.first;
                f = frees.back();
                frees.pop_back();
                return reinterpret_cast<void*>(ptr);
            }
        }
    }

    // need a new allocation
    void* ptr = malloc(sz ? sz : 1);
    if (ptr) {
        allocs[reinterpret_cast<uintptr_t>(ptr)] = sz;
    }
    return ptr;
}

void base_free(void* ptr) {
    if (disabled || !ptr) {
        free(ptr);
    } else {
        // mark free if found; if not found, invalid free: silently ignore
        auto it = allocs.find(reinterpret_cast<uintptr_t>(ptr));
        if (it != allocs.end()) {
            frees.push_back(*it);
            allocs.erase(it);
        }
    }
}

void base_allocate_disable(int d) {
    disabled = d;
}

static void base_allocate_atexit() {
    // clean up freed memory to shut up leak detector
    for (auto& alloc : frees) {
        free(reinterpret_cast<void*>(alloc.first));
    }
}
