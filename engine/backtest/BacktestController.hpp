#pragma once

#include "benchmarks/LatencyHistogram.hpp"
#include "engine/backtest/ResultAggregator.hpp"
#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/replay/ReplayController.hpp"
#include "engine/strategy/StrategyEngine.hpp"

#include <cstdint>
#include <vector>

namespace trading {

enum class ReplayMode : uint8_t {
    RealTime,     // replay at recorded timestamps (wall-clock pacing)
    Accelerated,  // replay N × faster than original speed
    Unlimited,    // replay as fast as hardware allows (default)
};

struct BacktestRunConfig {
    StrategyConfig strategy_config;
    uint32_t       run_number{0};
};

// Orchestrates single and multi-run backtests over an existing ReplayController.
// Does NOT modify ReplayController or ReplayEngine internals.
//
// Phase 5: native per-event latency histogram — no external feed required.
// Phase 5: ReplayMode pacing — RealTime and Accelerated modes honour
//          event timestamps; Unlimited runs as fast as hardware allows.
//
// Hierarchy:
//   BacktestController → ReplayController → ReplayEngine
class BacktestController {
public:
    struct Config {
        std::vector<BacktestRunConfig> runs;
        ReplayMode mode{ReplayMode::Unlimited};
        double acceleration_factor{1.0};  // multiplier for Accelerated mode
    };

    BacktestController(ReplayController& rc,
                       OrderBook&        book,
                       StrategyEngine&   strategy,
                       MatchingEngine&   matcher,
                       ResultAggregator& aggregator);

    // Execute all configured runs sequentially.
    // RESET is injected through the pipeline between consecutive runs.
    std::vector<RunResult> run_all(const Config& cfg);

    // Execute a single run: rewinds replay source, processes all events,
    // returns the aggregated result.
    RunResult run_single(const BacktestRunConfig& run_cfg,
                         ReplayMode mode = ReplayMode::Unlimited,
                         double acceleration_factor = 1.0);

    uint32_t runs_completed() const noexcept { return runs_completed_; }

    // Latency histogram populated during run_single() / run_all().
    // p50/p99/p999/max are valid after each run completes.
    const bench::LatencyHistogram& latency_histogram() const noexcept {
        return latency_;
    }

private:
    void process_event(const Event& ev) noexcept;

    // Nanosecond wall-clock — CLOCK_MONOTONIC_RAW on Linux, chrono elsewhere.
    static uint64_t now_ns() noexcept;

    // Sleep until wall_start_ns + (event_ts_ns / factor) has elapsed.
    static void pace(uint64_t wall_start_ns, uint64_t first_event_ts_ns,
                     uint64_t event_ts_ns, double factor) noexcept;

    ReplayController&      rc_;
    OrderBook&             book_;
    StrategyEngine&        strategy_;
    MatchingEngine&        matcher_;
    ResultAggregator&      aggregator_;
    bench::LatencyHistogram latency_;
    uint32_t               runs_completed_{0};
};

} // namespace trading
