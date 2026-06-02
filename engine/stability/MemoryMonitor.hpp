#pragma once

#include <cstddef>
#include <cstdint>

namespace trading {

// Tracks process RSS (Resident Set Size) to detect memory leaks during
// long-duration stability runs.
//
// Source: /proc/self/status → VmRSS  (Linux only; returns 0 elsewhere)
// Sampling: call record() periodically (spec: every 60 s from StabilityRunner).
//
// Dashboard must display:
//   current RSS, peak RSS, growth rate (MB/hour), projected 24-hour growth
class MemoryMonitor {
public:
    // Read current VmRSS. Returns 0 on non-Linux or if /proc/self/status is
    // unreadable.
    static std::size_t current_rss_mb() noexcept;

    // Capture the current RSS as the baseline for growth tracking.
    // Call once after the warm-up period.
    void set_baseline() noexcept;

    // Record a new sample. Updates current + peak values.
    void record() noexcept;

    std::size_t current_mb()  const noexcept { return current_mb_; }
    std::size_t peak_mb()     const noexcept { return peak_mb_; }
    std::size_t baseline_mb() const noexcept { return baseline_mb_; }

    // current_mb - baseline_mb (clamped to 0 if baseline > current).
    std::size_t growth_mb() const noexcept;

    // Growth rate in MB/hour derived from elapsed time since set_baseline().
    double growth_rate_mb_per_hour() const noexcept;

    // Projected increase over the next 24 hours at the current growth rate.
    double projected_24h_growth_mb() const noexcept;

    void reset() noexcept;

private:
    static uint64_t now_ns() noexcept;

    std::size_t current_mb_{0};
    std::size_t peak_mb_{0};
    std::size_t baseline_mb_{0};
    uint64_t    baseline_time_ns_{0};
    uint64_t    current_time_ns_{0};
};

} // namespace trading
