#pragma once

#include <cstddef>

// Huge-page backed raw allocator.
// On Linux with huge pages configured: uses mmap(MAP_HUGETLB).
// On WSL2 or when huge pages are not available: silently falls back to
// standard malloc so nothing breaks.

namespace trading {

class HugePageAllocator {
public:
    // True when MAP_HUGETLB allocation succeeds (probed at first call).
    static bool available() noexcept;

    // Allocate `bytes` using huge pages if available; otherwise malloc.
    // `bytes` is rounded up to the nearest huge-page boundary internally.
    // Returns nullptr on failure; never throws.
    static void* allocate(std::size_t bytes) noexcept;

    // Release memory returned by allocate().
    static void  deallocate(void* ptr, std::size_t bytes) noexcept;

    // Huge page size in bytes (typically 2 MiB).
    static std::size_t page_size() noexcept;

private:
    static std::size_t round_up(std::size_t bytes) noexcept;
};

} // namespace trading
