#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

// ── Runtime allocation guard ─────────────────────────────────────────────────
//
// When g_engine_running is true, any call to operator new increments
// g_unexpected_alloc_count and aborts in debug builds.
//
// Usage pattern:
//
//   // startup phase — allocations allowed
//   alloc.start(cfg);
//   trading::g_engine_running.store(true);
//
//   // hot path — zero unexpected allocations expected
//   while (rc.next(event)) { ... }
//
//   trading::g_engine_running.store(false);
//
// In tests, wrap replay with AllocationGuard RAII to assert zero allocations:
//
//   trading::AllocationGuard guard;
//   run_replay(...);
//   EXPECT_EQ(guard.count(), 0u);

namespace trading {

inline std::atomic<bool>   g_engine_running{false};
inline std::atomic<size_t> g_unexpected_alloc_count{0};

// RAII helper: sets g_engine_running for the duration of its lifetime,
// then resets it. Reports the unexpected allocation count on destruction.
class AllocationGuard {
public:
    AllocationGuard() {
        g_unexpected_alloc_count.store(0, std::memory_order_relaxed);
        g_engine_running.store(true, std::memory_order_seq_cst);
    }
    ~AllocationGuard() {
        g_engine_running.store(false, std::memory_order_seq_cst);
    }

    size_t count() const noexcept {
        return g_unexpected_alloc_count.load(std::memory_order_relaxed);
    }

    AllocationGuard(const AllocationGuard&)            = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;
};

} // namespace trading

// ── operator new overrides ────────────────────────────────────────────────────
//
// Defined in AllocationGuard.cpp — only one translation unit must define them.
// Including this header in test files gives access to the guard without
// re-defining the operators.
