#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace trading {

struct StabilityThresholds {
    double      max_latency_drift_pct    = 5.0;   // alert when latency drifts > 5%
    double      max_throughput_drift_pct = 5.0;   // alert when throughput drops > 5%
    std::size_t max_memory_growth_mb     = 100;   // alert when RSS grows > 100 MB
    std::size_t max_queue_depth_pct      = 80;    // alert when queue > 80% full
    std::size_t max_pool_exhaustions     = 0;     // any pool exhaustion = defect
};

struct StabilityAlert {
    enum class Kind {
        LatencyDrift,
        ThroughputDrop,
        MemoryGrowth,
        QueueDepth,
        PoolExhaustion,
    };
    Kind        kind;
    double      observed_value;
    double      threshold_value;
    std::string message;
};

// Continuously detects latency drift, throughput drift, memory growth,
// queue depth growth, and pool exhaustion events.
//
// Usage:
//   1. Record initial measurements and call set_baseline().
//   2. Periodically record updated measurements.
//   3. Call check() to get a list of active threshold breaches.
class StabilityMonitor {
public:
    explicit StabilityMonitor(StabilityThresholds thresholds = {});

    // ── Feed measurements (call periodically) ─────────────────────────────

    void record_latency_p99(uint64_t p99_ns) noexcept;
    void record_throughput(double eps) noexcept;
    void record_memory_mb(std::size_t rss_mb) noexcept;
    void record_queue_depth_pct(double pct) noexcept;
    void record_pool_exhaustions(std::size_t total) noexcept;

    // Capture the current latency + throughput as the baseline for drift.
    // Call once after the warm-up period.
    void set_baseline() noexcept;

    // ── Computed metrics ──────────────────────────────────────────────────

    // (current − baseline) / baseline × 100.
    // Latency: positive = slower (regression).
    // Throughput: negative = slower (regression).
    double      latency_drift_pct()    const noexcept;
    double      throughput_drift_pct() const noexcept;
    std::size_t memory_growth_mb()     const noexcept;
    double      queue_depth_pct()      const noexcept { return current_queue_pct_; }
    std::size_t pool_exhaustions()     const noexcept { return total_pool_exhaustions_; }

    // ── Threshold checking ────────────────────────────────────────────────

    // Returns one StabilityAlert per breached threshold.
    std::vector<StabilityAlert> check() const;
    bool is_stable() const;

    const StabilityThresholds& thresholds() const noexcept { return thresholds_; }

    void reset() noexcept;

private:
    StabilityThresholds thresholds_;

    // Baselines (set after warm-up)
    uint64_t    baseline_p99_ns_{0};
    double      baseline_throughput_{0.0};
    std::size_t baseline_memory_mb_{0};

    // Current values
    uint64_t    current_p99_ns_{0};
    double      current_throughput_{0.0};
    std::size_t current_memory_mb_{0};
    double      current_queue_pct_{0.0};
    std::size_t total_pool_exhaustions_{0};
};

} // namespace trading
