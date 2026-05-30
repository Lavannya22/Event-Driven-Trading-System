#pragma once

#include "benchmarks/LatencyHistogram.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace trading::bench {

// Live performance metrics for dashboard integration.
// Updated from the engine hot path (lock-free atomics only).
// Read by the dashboard broadcast thread (relaxed loads, snapshot copy).
struct PerformanceMetrics {
    std::atomic<uint64_t> events_processed{0};
    std::atomic<uint64_t> total_duration_ns{0};

    // Running throughput — updated every N events by the hot path.
    std::atomic<uint64_t> throughput_eps{0};    // events/sec (integer)

    // Queue and pool pressure
    std::atomic<uint64_t> queue_depth{0};
    std::atomic<uint64_t> pool_exhaustion_count{0};

    // Latency histogram (lock-free, accumulated across all events).
    LatencyHistogram latency;

    // Snapshot for dashboard — POD, no atomics (taken under brief mutex).
    struct Snapshot {
        uint64_t events_processed{0};
        double   throughput_eps{0};
        uint64_t p50_ns{0};
        uint64_t p95_ns{0};
        uint64_t p99_ns{0};
        uint64_t p999_ns{0};
        uint64_t max_ns{0};
        uint64_t queue_depth{0};
        uint64_t pool_exhaustion_count{0};
    };

    Snapshot snapshot() const noexcept {
        Snapshot s;
        s.events_processed     = events_processed.load(std::memory_order_relaxed);
        s.throughput_eps       = static_cast<double>(
                                     throughput_eps.load(std::memory_order_relaxed));
        s.p50_ns               = latency.p50();
        s.p95_ns               = latency.p95();
        s.p99_ns               = latency.p99();
        s.p999_ns              = latency.p999();
        s.max_ns               = latency.max_ns();
        s.queue_depth          = queue_depth.load(std::memory_order_relaxed);
        s.pool_exhaustion_count= pool_exhaustion_count.load(std::memory_order_relaxed);
        return s;
    }

    void reset() noexcept {
        events_processed.store(0, std::memory_order_relaxed);
        total_duration_ns.store(0, std::memory_order_relaxed);
        throughput_eps.store(0, std::memory_order_relaxed);
        queue_depth.store(0, std::memory_order_relaxed);
        pool_exhaustion_count.store(0, std::memory_order_relaxed);
        latency.reset();
    }
};

} // namespace trading::bench
