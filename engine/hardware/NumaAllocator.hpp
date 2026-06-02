#pragma once

#include <cstddef>
#include <cstdlib>

// NUMA-aware raw allocator.
// On Linux with libnuma: allocates from the requested NUMA node.
// On WSL2 or non-Linux: silently falls back to standard malloc/free.
// node = -1 means "any node" (standard allocation).

namespace trading {

class NumaAllocator {
public:
    // Returns true when real NUMA support is compiled in and available.
    static bool available() noexcept;

    // Allocate `bytes` on NUMA `node` (-1 = any).
    // Returns aligned memory; never throws — returns nullptr on failure.
    static void* allocate(std::size_t bytes, int node = -1) noexcept;

    // Release memory previously returned by allocate().
    static void  deallocate(void* ptr, std::size_t bytes) noexcept;

    // Which NUMA node a pointer was allocated from (-1 = unknown / not NUMA).
    static int   node_of(void* ptr) noexcept;
};

} // namespace trading
