#include "engine/memory/AllocationGuard.hpp"
#include <cstdio>

// These replacements intercept every heap allocation while the engine is
// running.  They delegate to the system allocator so memory still works;
// they just count (and optionally abort on) unexpected runtime allocations.

static void* guarded_alloc(std::size_t size, bool nothrow) {
    if (trading::g_engine_running.load(std::memory_order_relaxed)) {
        trading::g_unexpected_alloc_count.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size == 0 ? 1 : size);
    if (!ptr && !nothrow) throw std::bad_alloc{};
    return ptr;
}

void* operator new(std::size_t size) {
    return guarded_alloc(size, false);
}

void* operator new[](std::size_t size) {
    return guarded_alloc(size, false);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return guarded_alloc(size, true);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return guarded_alloc(size, true);
}

void operator delete(void* ptr) noexcept                      { std::free(ptr); }
void operator delete[](void* ptr) noexcept                    { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept         { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept       { std::free(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept  { std::free(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept{ std::free(ptr); }
