#include "engine/backtest/BacktestController.hpp"

namespace trading {

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

void BacktestController::process_event(const Event& ev) noexcept {
    if (event_type(ev) == EventType::RESET) {
        // RESET workflow (steps 3-5 of spec):
        //   3. Reset order books
        //   4. Reset strategy state
        //   5. Reset metrics (handled by aggregator_.begin_run() at run start)
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

RunResult BacktestController::run_single(const BacktestRunConfig& run_cfg) {
    // begin_run resets aggregator state and captures the new config.
    aggregator_.begin_run(run_cfg.run_number, run_cfg.strategy_config);
    rc_.reset();  // rewind replay source to beginning

    Event ev{};
    while (rc_.next(ev)) {
        process_event(ev);
    }

    ++runs_completed_;
    return aggregator_.finish_run();
}

std::vector<RunResult> BacktestController::run_all(const Config& cfg) {
    std::vector<RunResult> results;
    results.reserve(cfg.runs.size());

    for (const auto& run_cfg : cfg.runs) {
        // Inject a RESET event through the pipeline between consecutive runs.
        // Step 1-2 of the RESET workflow: inject RESET + drain (single-threaded:
        // process_event is the drain — the RESET event IS the pipeline signal).
        if (!results.empty())
            process_event(make_reset_event(0));

        results.push_back(run_single(run_cfg));
    }

    return results;
}

} // namespace trading
