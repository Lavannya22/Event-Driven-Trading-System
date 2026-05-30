#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

// Lock-free HDR-style latency histogram using log2 buckets.
//
// Bucket i covers the nanosecond range [2^(i-1), 2^i).
// 64 buckets span 1 ns → ~9.2e18 ns (effectively unlimited ceiling).
// O(1) insertion via __builtin_clzll — no division, no allocation.
//
// All storage is pre-allocated in the object — zero runtime heap use.
// Thread-safe: each bucket is an independent atomic, so concurrent
// record() calls from multiple threads never contend.

namespace trading::bench {

class LatencyHistogram {
public:
    static constexpr std::size_t BUCKETS = 64;

    void record(uint64_t ns) noexcept {
        counts_[bucket_index(ns)].fetch_add(1, std::memory_order_relaxed);
        total_.fetch_add(1, std::memory_order_relaxed);
        // Track max without lock — relaxed CAS loop
        uint64_t cur = max_.load(std::memory_order_relaxed);
        while (ns > cur &&
               !max_.compare_exchange_weak(cur, ns,
                   std::memory_order_relaxed, std::memory_order_relaxed))
        {}
    }

    void reset() noexcept {
        for (auto& c : counts_) c.store(0, std::memory_order_relaxed);
        total_.store(0, std::memory_order_relaxed);
        max_.store(0, std::memory_order_relaxed);
    }

    uint64_t total_count() const noexcept {
        return total_.load(std::memory_order_relaxed);
    }

    uint64_t max_ns() const noexcept {
        return max_.load(std::memory_order_relaxed);
    }

    // Returns the approximate nanosecond value at the given percentile (0–100).
    uint64_t percentile(double p) const noexcept {
        const uint64_t n = total_.load(std::memory_order_relaxed);
        if (n == 0) return 0;

        const uint64_t target = static_cast<uint64_t>(static_cast<double>(n) * p / 100.0);
        uint64_t cumulative = 0;

        for (std::size_t i = 0; i < BUCKETS; ++i) {
            cumulative += counts_[i].load(std::memory_order_relaxed);
            if (cumulative > target)
                return representative(i);
        }
        return representative(BUCKETS - 1);
    }

    uint64_t p50()  const noexcept { return percentile(50.0);  }
    uint64_t p95()  const noexcept { return percentile(95.0);  }
    uint64_t p99()  const noexcept { return percentile(99.0);  }
    uint64_t p999() const noexcept { return percentile(99.9);  }

    // Count in a specific bucket (for debugging / dashboard).
    uint64_t bucket_count(std::size_t i) const noexcept {
        return i < BUCKETS ? counts_[i].load(std::memory_order_relaxed) : 0;
    }

private:
    // Map nanoseconds to a bucket index using floor(log2(ns)).
    static std::size_t bucket_index(uint64_t ns) noexcept {
        if (ns == 0) return 0;
        // __builtin_clzll counts leading zeros in a 64-bit value.
        // 63 - clz gives floor(log2(ns)).
        const int lz = __builtin_clzll(ns);
        const std::size_t idx = static_cast<std::size_t>(63 - lz);
        return idx < BUCKETS ? idx : BUCKETS - 1;
    }

    // Mid-point of the bucket range as the representative nanosecond value.
    // Bucket i spans [2^(i-1), 2^i); mid-point = 3 * 2^(i-2).
    static uint64_t representative(std::size_t i) noexcept {
        if (i == 0) return 1;
        const uint64_t lo = (1ull << (i - 1));
        const uint64_t hi = (1ull << i) - 1;
        return (lo + hi) / 2;
    }

    alignas(64) std::array<std::atomic<uint64_t>, BUCKETS> counts_{};
    alignas(8)  std::atomic<uint64_t> total_{0};
    alignas(8)  std::atomic<uint64_t> max_{0};
};

} // namespace trading::bench
