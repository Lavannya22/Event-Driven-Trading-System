#include "engine/hardware/NumaAllocator.hpp"

// libnuma is optional.  When WITH_NUMA is not defined (default on WSL2 and
// non-Linux) every method degrades to malloc/free transparently.
#ifdef WITH_NUMA
#  include <numa.h>
#  include <numaif.h>
#endif

#include <cstring>

namespace trading {

bool NumaAllocator::available() noexcept {
#ifdef WITH_NUMA
    return numa_available() >= 0;
#else
    return false;
#endif
}

void* NumaAllocator::allocate(std::size_t bytes, int node) noexcept {
    if (bytes == 0) bytes = 1;
#ifdef WITH_NUMA
    if (node >= 0 && available()) {
        void* ptr = numa_alloc_onnode(bytes, node);
        if (ptr) return ptr;
        // Fall through to malloc on NUMA failure
    }
#endif
    (void)node;
    return std::malloc(bytes);
}

void NumaAllocator::deallocate(void* ptr, std::size_t bytes) noexcept {
    if (!ptr) return;
#ifdef WITH_NUMA
    if (available()) {
        numa_free(ptr, bytes);
        return;
    }
#endif
    (void)bytes;
    std::free(ptr);
}

int NumaAllocator::node_of(void* ptr) noexcept {
#ifdef WITH_NUMA
    if (!ptr || !available()) return -1;
    int node = -1;
    get_mempolicy(&node, nullptr, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR);
    return node;
#else
    (void)ptr;
    return -1;
#endif
}

} // namespace trading
