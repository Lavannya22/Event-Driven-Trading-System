#pragma once

#include "engine/backtest/BacktestController.hpp"
#include "engine/stability/MemoryMonitor.hpp"
#include "engine/stability/StabilityMonitor.hpp"
#include <cstdint>
#include <vector>

namespace trading {

// Drives a long-duration replay for stability validation.
//
// Supported durations: 1h / 6h / 12h / 24h
//
// The runner loops BacktestController::run_all() until the wall-clock duration
// (or max_events override) is reached, sampling memory every
// memory_sample_interval_s seconds and forwarding metrics to StabilityMonitor.
//
// Validation metrics tracked throughout:
//   latency drift, throughput drift, memory growth,
//   queue depth growth, pool utilization
class StabilityRunner {
public:
    enum class Duration : uint32_t {
        OneHour        = 3600,
        SixHour        = 21600,
        TwelveHour     = 43200,
        TwentyFourHour = 86400,
    };

    struct Config {
        Duration duration{Duration::OneHour};
        // If > 0, stop after this many events regardless of wall-clock time.
        // Useful for unit testing — avoids sleeping for an hour.
        uint64_t max_events{0};
        // RSS sampling interval in seconds (spec: 60 s).
        uint32_t memory_sample_interval_s{60};
    };

    struct Report {
        bool        passed{true};
        double      latency_drift_pct{0.0};
        double      throughput_drift_pct{0.0};
        std::size_t memory_growth_mb{0};
        double      projected_24h_growth_mb{0.0};
        double      peak_queue_depth_pct{0.0};
        std::size_t pool_exhaustions{0};
        uint64_t    events_processed{0};
        double      actual_duration_s{0.0};
        std::vector<StabilityAlert> alerts;
    };

    // Run the stability validation.
    // Drives `controller.run_all(bt_cfg)` in a loop until the configured
    // duration or max_events limit is reached.
    Report run(BacktestController&               controller,
               const BacktestController::Config& bt_cfg,
               StabilityMonitor&                 monitor,
               MemoryMonitor&                    memory,
               const Config&                     cfg);

private:
    static uint64_t now_ns() noexcept;
};

} // namespace trading
