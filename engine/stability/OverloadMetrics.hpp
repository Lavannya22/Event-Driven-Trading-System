#pragma once

#include <atomic>
#include <cstdint>

namespace trading {

// Lock-free overload counters updated from the hot path.
// Read by the dashboard snapshot thread (relaxed loads — approximate is fine).
//
// Exposed to the dashboard backpressure panel as:
//   queue_rejections      — SPSCQueue::enqueue() returned false
//   persistence_overflows — PostgresWriter dropped a write (queue full)
// Pool exhaustions come from ObjectPool::exhaustion_count().
struct OverloadMetrics {
    std::atomic<uint64_t> queue_rejections{0};
    std::atomic<uint64_t> persistence_overflows{0};

    void reset() noexcept {
        queue_rejections.store(0, std::memory_order_relaxed);
        persistence_overflows.store(0, std::memory_order_relaxed);
    }

    struct Snapshot {
        uint64_t queue_rejections{0};
        uint64_t persistence_overflows{0};
    };

    Snapshot snapshot() const noexcept {
        return {
            queue_rejections.load(std::memory_order_relaxed),
            persistence_overflows.load(std::memory_order_relaxed)
        };
    }
};

} // namespace trading
