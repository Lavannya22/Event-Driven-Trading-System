#pragma once

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
    Unlimited,    // replay as fast as hardware allows
};

struct BacktestRunConfig {
    StrategyConfig strategy_config;
    uint32_t       run_number{0};
};

// Orchestrates single and multi-run backtests over an existing ReplayController.
// Does NOT modify ReplayController or ReplayEngine internals.
//
// State reset between runs flows through the event pipeline via a synthetic
// RESET event — no thread restarts, no out-of-band sync, no runtime allocation.
//
// Hierarchy:
//   BacktestController → ReplayController → ReplayEngine
class BacktestController {
public:
    struct Config {
        std::vector<BacktestRunConfig> runs;
        ReplayMode mode{ReplayMode::Unlimited};
        double acceleration_factor{1.0};
    };

    // All referenced components must outlive the controller.
    BacktestController(ReplayController& rc,
                       OrderBook&        book,
                       StrategyEngine&   strategy,
                       MatchingEngine&   matcher,
                       ResultAggregator& aggregator);

    // Execute all configured runs sequentially.
    // A RESET event is injected through the pipeline between consecutive runs.
    // Returns one RunResult per run in order.
    std::vector<RunResult> run_all(const Config& cfg);

    // Execute a single run: rewinds the replay source, processes all events,
    // and returns the aggregated result.
    RunResult run_single(const BacktestRunConfig& run_cfg);

    uint32_t runs_completed() const noexcept { return runs_completed_; }

private:
    // Dispatch one event through the pipeline.
    // RESET triggers the reset workflow (book + strategy counters).
    // All other events flow to strategy → matcher → aggregator.
    void process_event(const Event& ev) noexcept;

    ReplayController& rc_;
    OrderBook&        book_;
    StrategyEngine&   strategy_;
    MatchingEngine&   matcher_;
    ResultAggregator& aggregator_;
    uint32_t          runs_completed_{0};
};

} // namespace trading
