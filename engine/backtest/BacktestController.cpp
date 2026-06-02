#include "engine/backtest/BacktestController.hpp"

#include <chrono>
#include <thread>

#ifdef __linux__
#  include <time.h>
#endif

namespace trading {

// ── Helpers ───────────────────────────────────────────────────────────────────

uint64_t BacktestController::now_ns() noexcept {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ull +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(
            steady_clock::now().time_since_epoch()).count());
#endif
}

void BacktestController::pace(uint64_t wall_start_ns,
                               uint64_t first_event_ts_ns,
                               uint64_t event_ts_ns,
                               double   factor) noexcept {
    if (factor <= 0.0) return;
    const uint64_t event_elapsed = event_ts_ns - first_event_ts_ns;
    const uint64_t target_wall   =
        wall_start_ns +
        static_cast<uint64_t>(static_cast<double>(event_elapsed) / factor);
    const uint64_t now           = now_ns();
    if (now < target_wall) {
        const uint64_t sleep_ns = target_wall - now;
        std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

BacktestController::BacktestController(ReplayController& rc,
                                        OrderBook&        book,
                                        StrategyEngine&   strategy,
                                        MatchingEngine&   matcher,
                                        ResultAggregator& aggregator)
    : rc_(rc)
    , book_(book)
    , strategy_(strategy)
    , matcher_(matcher)
    , aggregator_(aggregator)
{}

// ── process_event ─────────────────────────────────────────────────────────────

void BacktestController::process_event(const Event& ev) noexcept {
    if (event_type(ev) == EventType::RESET) {
        // RESET workflow: reset order book + strategy counters.
        // No allocation, no thread restart, no out-of-band sync.
        book_.reset();
        strategy_.reset_counters();
        return;
    }

    aggregator_.on_event();

    if (strategy_.process(ev) != StrategySignal::Signal) return;

    const auto etype = event_type(ev);
    if (etype == EventType::NewOrder) {
        auto out = matcher_.process_new_order(book_, ev);
        for (const auto& e : out.event_span()) {
            if (event_type(e) == EventType::TradeExecution)
                aggregator_.on_trade(e);
        }
    } else if (etype == EventType::CancelOrder) {
        matcher_.process_cancel(book_, ev);
    } else if (etype == EventType::ModifyOrder) {
        matcher_.process_modify(book_, ev);
    }
}

// ── run_single ────────────────────────────────────────────────────────────────

RunResult BacktestController::run_single(const BacktestRunConfig& run_cfg,
                                          ReplayMode  mode,
                                          double      acceleration_factor) {
    aggregator_.begin_run(run_cfg.run_number, run_cfg.strategy_config);
    latency_.reset();
    rc_.reset();

    const bool real_time   = (mode == ReplayMode::RealTime);
    const bool accelerated = (mode == ReplayMode::Accelerated);
    const bool pace_events = real_time || accelerated;
    const double factor    = real_time ? 1.0 : acceleration_factor;

    uint64_t wall_start_ns    = 0;
    uint64_t first_event_ts   = 0;
    bool     first_event_seen = false;

    Event ev{};
    while (rc_.next(ev)) {
        // Pacing: honour event timestamps for RealTime / Accelerated modes.
        if (pace_events && event_type(ev) != EventType::RESET) {
            if (!first_event_seen) {
                wall_start_ns  = now_ns();
                first_event_ts = ev.timestamp;
                first_event_seen = true;
            } else if (ev.timestamp > first_event_ts) {
                pace(wall_start_ns, first_event_ts, ev.timestamp, factor);
            }
        }

        // Measure hot-path latency per event (Phase 5 gap closure).
        const uint64_t t0 = now_ns();
        process_event(ev);
        const uint64_t t1 = now_ns();

        if (event_type(ev) != EventType::RESET)
            latency_.record(t1 - t0);
    }

    ++runs_completed_;
    return aggregator_.finish_run();
}

// ── run_all ───────────────────────────────────────────────────────────────────

std::vector<RunResult> BacktestController::run_all(const Config& cfg) {
    std::vector<RunResult> results;
    results.reserve(cfg.runs.size());

    for (const auto& run_cfg : cfg.runs) {
        if (!results.empty())
            process_event(make_reset_event(0));

        results.push_back(
            run_single(run_cfg, cfg.mode, cfg.acceleration_factor));
    }

    return results;
}

} // namespace trading
