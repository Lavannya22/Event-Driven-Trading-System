#include "engine/hardware/HugePageAllocator.hpp"
#include <cstdlib>

#ifdef __linux__
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace trading {

namespace {
    constexpr std::size_t kDefaultHugePage = 2UL * 1024 * 1024;  // 2 MiB

    // Probe once whether MAP_HUGETLB actually works on this system.
    bool probe_huge_pages() noexcept {
#ifdef __linux__
        void* p = mmap(nullptr, kDefaultHugePage,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                       -1, 0);
        if (p == MAP_FAILED) return false;
        munmap(p, kDefaultHugePage);
        return true;
#else
        return false;
#endif
    }
} // namespace

bool HugePageAllocator::available() noexcept {
    static const bool ok = probe_huge_pages();
    return ok;
}

std::size_t HugePageAllocator::page_size() noexcept {
    return kDefaultHugePage;
}

std::size_t HugePageAllocator::round_up(std::size_t bytes) noexcept {
    const std::size_t ps = page_size();
    return (bytes + ps - 1) & ~(ps - 1);
}

void* HugePageAllocator::allocate(std::size_t bytes) noexcept {
    if (bytes == 0) bytes = 1;
#ifdef __linux__
    if (available()) {
        const std::size_t size = round_up(bytes);
        void* p = mmap(nullptr, size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                       -1, 0);
        if (p != MAP_FAILED) return p;
        // Fall through to malloc
    }
#endif
    return std::malloc(bytes);
}

void HugePageAllocator::deallocate(void* ptr, std::size_t bytes) noexcept {
    if (!ptr) return;
#ifdef __linux__
    if (available() && bytes > 0) {
        munmap(ptr, round_up(bytes));
        return;
    }
#endif
    (void)bytes;
    std::free(ptr);
}

} // namespace trading
