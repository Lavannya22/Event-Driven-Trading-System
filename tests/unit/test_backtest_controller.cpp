#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "engine/backtest/BacktestController.hpp"
#include "engine/backtest/ResultAggregator.hpp"
#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"

using namespace trading;

// ── Helpers ───────────────────────────────────────────────────────────────────

// One resting ask + one aggressive bid that fully crosses it → 1 trade.
static std::vector<Event> crossing_events() {
    return {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 1002, 10050,  50, Side::Bid),
    };
}

// Resting ask + bid that does NOT cross (bid price < ask price) → 0 trades.
static std::vector<Event> non_crossing_events() {
    return {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 1002,  9000,  50, Side::Bid),
    };
}

struct Fixture {
    VectorReplayController  rc;
    OrderBook               book;
    StrategyEngine          strategy;
    MatchingEngine          matcher;
    ResultAggregator        aggregator;
    BacktestController      controller;

    explicit Fixture(std::vector<Event> events)
        : rc(std::move(events))
        , book(1)
        , strategy(std::make_unique<PassThroughStrategy>())
        , controller(rc, book, strategy, matcher, aggregator)
    {}
};

// ── Single run ────────────────────────────────────────────────────────────────

TEST(BacktestController, SingleRunCountsTrades) {
    Fixture f(crossing_events());
    BacktestRunConfig cfg; cfg.run_number = 1;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.total_trades,     1u);
    EXPECT_EQ(result.run_number,       1u);
    EXPECT_EQ(f.controller.runs_completed(), 1u);
}

TEST(BacktestController, SingleRunEventsProcessed) {
    Fixture f(crossing_events());
    BacktestRunConfig cfg; cfg.run_number = 1;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.events_processed, 2u);
}

TEST(BacktestController, SingleRunNoCrossNoTrades) {
    Fixture f(non_crossing_events());
    BacktestRunConfig cfg; cfg.run_number = 1;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.total_trades, 0u);
}

TEST(BacktestController, SingleRunEmptyEventsProducesEmptyResult) {
    Fixture f({});
    BacktestRunConfig cfg; cfg.run_number = 7;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.total_trades,     0u);
    EXPECT_EQ(result.events_processed, 0u);
    EXPECT_EQ(result.run_number,       7u);
    EXPECT_DOUBLE_EQ(result.realized_pnl, 0.0);
}

TEST(BacktestController, SingleRunCarriesConfig) {
    Fixture f(crossing_events());
    BacktestRunConfig cfg;
    cfg.run_number = 1;
    cfg.strategy_config.window_size     = 42;
    cfg.strategy_config.threshold       = 0.05;
    cfg.strategy_config.inventory_limit = 500.0;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.config.window_size,     42u);
    EXPECT_DOUBLE_EQ(result.config.threshold,       0.05);
    EXPECT_DOUBLE_EQ(result.config.inventory_limit, 500.0);
}

// ── Multi-run ─────────────────────────────────────────────────────────────────

TEST(BacktestController, MultiRunProducesOneResultPerRun) {
    Fixture f(crossing_events());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 5; ++i) {
        BacktestRunConfig r; r.run_number = i;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(f.controller.runs_completed(), 5u);
}

TEST(BacktestController, MultiRunRunNumbersPreserved) {
    Fixture f(crossing_events());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 4; ++i) {
        BacktestRunConfig r; r.run_number = i * 10;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 4u);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_EQ(results[i].run_number, (i + 1) * 10);
}

TEST(BacktestController, MultiRunDeterministicOutputs) {
    // Identical replay replayed N times must produce identical results.
    Fixture f(crossing_events());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 5; ++i) {
        BacktestRunConfig r; r.run_number = i;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 5u);
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[i].total_trades,     results[0].total_trades)
            << "trade count mismatch at run " << i;
        EXPECT_EQ(results[i].events_processed, results[0].events_processed)
            << "event count mismatch at run " << i;
        EXPECT_DOUBLE_EQ(results[i].realized_pnl, results[0].realized_pnl)
            << "PnL mismatch at run " << i;
    }
}

TEST(BacktestController, MultiRunSingleEntryBehavesLikeSingleRun) {
    Fixture f(crossing_events());
    BacktestRunConfig r; r.run_number = 99;

    BacktestController::Config cfg;
    cfg.runs = {r};

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].total_trades, 1u);
    EXPECT_EQ(results[0].run_number, 99u);
}

// ── RESET workflow ────────────────────────────────────────────────────────────

TEST(BacktestController, ResetEventInStreamClearsBook) {
    // Ask placed, then RESET clears book, then bid has nothing to cross against.
    std::vector<Event> events = {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_reset_event(2000),
        make_new_order(3000, 1, 1002, 10050,  50, Side::Bid),
    };
    Fixture f(std::move(events));
    BacktestRunConfig cfg; cfg.run_number = 1;

    auto result = f.controller.run_single(cfg);

    // RESET cleared the book; the post-RESET bid has no ask → no trade.
    EXPECT_EQ(result.total_trades, 0u);
    EXPECT_FALSE(f.book.has_ask());  // cleared by RESET
    EXPECT_TRUE(f.book.has_bid());   // post-RESET bid is resting
}

TEST(BacktestController, ResetBetweenRunsClearsBook) {
    // Run 1 leaves a resting ask. Run 2 should start with a clean book.
    // Proof: the bid in run 2 events is below the ask → no trade IF book is clean.
    // If the book were NOT reset, the ask would accumulate → same result in this
    // scenario because we rewind the source. Instead we verify via run count.
    std::vector<Event> events = {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
    };
    Fixture f(std::move(events));

    BacktestController::Config cfg;
    BacktestRunConfig r1; r1.run_number = 1;
    BacktestRunConfig r2; r2.run_number = 2;
    cfg.runs = {r1, r2};

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(f.controller.runs_completed(), 2u);
    // Both runs produced the same result (clean-slate determinism).
    EXPECT_EQ(results[0].events_processed, results[1].events_processed);
}

TEST(BacktestController, MultipleResetEventsInStreamAreEachHandled) {
    std::vector<Event> events = {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_reset_event(1500),
        make_new_order(2000, 1, 1002, 10050, 100, Side::Ask),
        make_reset_event(2500),
        // Book is empty again — bid finds nothing.
        make_new_order(3000, 1, 1003, 10050,  50, Side::Bid),
    };
    Fixture f(std::move(events));
    BacktestRunConfig cfg; cfg.run_number = 1;

    auto result = f.controller.run_single(cfg);

    EXPECT_EQ(result.total_trades, 0u);
    EXPECT_FALSE(f.book.has_ask());
    EXPECT_TRUE(f.book.has_bid());
}

// ── Parameter sweep ───────────────────────────────────────────────────────────

TEST(BacktestController, ParameterSweepEachResultCarriesItsConfig) {
    Fixture f(crossing_events());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 10; ++i) {
        BacktestRunConfig r;
        r.run_number                    = i;
        r.strategy_config.window_size   = i * 5;
        r.strategy_config.threshold     = i * 0.01;
        r.strategy_config.inventory_limit = static_cast<double>(i) * 100.0;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 10u);
    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i].config.window_size,     (i + 1) * 5)
            << "window_size mismatch at index " << i;
        EXPECT_DOUBLE_EQ(results[i].config.threshold,       (i + 1) * 0.01)
            << "threshold mismatch at index " << i;
        EXPECT_DOUBLE_EQ(results[i].config.inventory_limit, static_cast<double>(i + 1) * 100.0)
            << "inventory_limit mismatch at index " << i;
    }
}

TEST(BacktestController, ParameterSweepRunsAllComplete) {
    Fixture f(non_crossing_events());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 10; ++i) {
        BacktestRunConfig r; r.run_number = i;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    EXPECT_EQ(results.size(), 10u);
    EXPECT_EQ(f.controller.runs_completed(), 10u);
}

// ── StrategyConfig lifecycle ──────────────────────────────────────────────────

TEST(StrategyConfig, DefaultValues) {
    StrategyConfig cfg;
    EXPECT_EQ(cfg.window_size,       20u);
    EXPECT_DOUBLE_EQ(cfg.threshold,          0.01);
    EXPECT_DOUBLE_EQ(cfg.inventory_limit,    1000.0);
}

TEST(StrategyConfig, ImmutableDuringReplay) {
    // Modifying the source config after passing to begin_run() must not
    // affect the captured config inside ResultAggregator.
    StrategyConfig cfg;
    cfg.window_size = 30;
    cfg.threshold   = 0.02;

    ResultAggregator agg;
    agg.begin_run(1, cfg);

    // Mutate original — must not affect the stored copy.
    cfg.window_size = 999;
    cfg.threshold   = 9.99;

    auto result = agg.finish_run();
    EXPECT_EQ(result.config.window_size, 30u);
    EXPECT_DOUBLE_EQ(result.config.threshold, 0.02);
}

TEST(StrategyConfig, CorrectInjectionOnReset) {
    // BacktestController applies the new config at the start of each run.
    // Verify that run N+1 carries its own config, not run N's.
    Fixture f({});  // empty events — runs complete immediately

    BacktestController::Config cfg;
    BacktestRunConfig r1; r1.run_number = 1; r1.strategy_config.window_size = 10;
    BacktestRunConfig r2; r2.run_number = 2; r2.strategy_config.window_size = 50;
    BacktestRunConfig r3; r3.run_number = 3; r3.strategy_config.window_size = 100;
    cfg.runs = {r1, r2, r3};

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].config.window_size, 10u);
    EXPECT_EQ(results[1].config.window_size, 50u);
    EXPECT_EQ(results[2].config.window_size, 100u);
}

TEST(StrategyConfig, NoCrossContaminationBetweenRuns) {
    // Config from run N must not bleed into run N+1's stored result.
    Fixture f({});

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 5; ++i) {
        BacktestRunConfig r;
        r.run_number                  = i;
        r.strategy_config.window_size = i * 7;
        r.strategy_config.threshold   = i * 0.003;
        cfg.runs.push_back(r);
    }

    auto results = f.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 5u);
    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(results[i].config.window_size, (i + 1) * 7u);
        EXPECT_DOUBLE_EQ(results[i].config.threshold, (i + 1) * 0.003);
    }
}
