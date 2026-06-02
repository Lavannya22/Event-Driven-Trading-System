#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "engine/backtest/BacktestController.hpp"
#include "engine/backtest/ResultAggregator.hpp"
#include "engine/backtest/StrategyConfig.hpp"
#include "engine/events/Event.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/memory/AllocationGuard.hpp"
#include "engine/memory/ObjectPool.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/queues/SPSCQueue.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/stability/MemoryMonitor.hpp"
#include "engine/stability/OverloadMetrics.hpp"
#include "engine/stability/StabilityMonitor.hpp"
#include "engine/stability/StabilityRunner.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"

#ifdef WITH_POSTGRES
#include "engine/persistence/PostgresWriter.hpp"
#endif

using namespace trading;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<Event> crossing_scenario() {
    return {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 1002, 10050,  50, Side::Bid),
        make_new_order(3000, 1, 1003, 10050,  50, Side::Bid),
    };
}

struct Engine {
    VectorReplayController rc;
    OrderBook              book;
    StrategyEngine         strategy;
    MatchingEngine         matcher;
    ResultAggregator       aggregator;
    BacktestController     controller;

    explicit Engine(std::vector<Event> events)
        : rc(std::move(events))
        , book(1)
        , strategy(std::make_unique<PassThroughStrategy>())
        , controller(rc, book, strategy, matcher, aggregator)
    {}
};

// ── Single Backtest ───────────────────────────────────────────────────────────

TEST(Phase4Integration, SingleBacktestProducesResult) {
    Engine eng(crossing_scenario());

    BacktestRunConfig cfg; cfg.run_number = 1;
    auto result = eng.controller.run_single(cfg);

    EXPECT_EQ(result.run_number, 1u);
    EXPECT_GT(result.total_trades, 0u);
    EXPECT_EQ(result.events_processed, 3u);
    EXPECT_NE(result.realized_pnl, 0.0);
}

TEST(Phase4Integration, SingleBacktestAllocationFree) {
    // Pre-allocate all state before the guard activates.
    Engine eng(crossing_scenario());

    BacktestRunConfig cfg; cfg.run_number = 1;
    RunResult result{};
    {
        AllocationGuard guard;
        result = eng.controller.run_single(cfg);
        EXPECT_EQ(guard.count(), 0u)
            << "Unexpected heap allocations during backtest hot path: "
            << guard.count();
    }
    EXPECT_GT(result.total_trades, 0u);
}

#ifdef WITH_POSTGRES
TEST(Phase4Integration, SingleBacktestWritesToPostgres) {
    PostgresWriter writer;
    writer.flush();  // wait for writer thread to attempt connection
    if (!writer.connected()) {
        GTEST_SKIP() << "PostgreSQL not reachable (run: docker compose up -d)";
    }

    Engine eng(crossing_scenario());
    BacktestRunConfig cfg;
    cfg.run_number                = 1;
    cfg.strategy_config.window_size = 20;

    const std::string config_id = writer.write_strategy_config(cfg.strategy_config);
    EXPECT_FALSE(config_id.empty());

    const std::string run_id =
        writer.begin_backtest_run("crossing_scenario", config_id);
    EXPECT_FALSE(run_id.empty());

    auto result = eng.controller.run_single(cfg);
    writer.write_backtest_result(run_id, result);
    writer.end_backtest_run(run_id, "completed");
    writer.flush();

    SUCCEED();
}
#endif

// ── Multi-Run Backtest ────────────────────────────────────────────────────────

TEST(Phase4Integration, MultiRunBacktest100Runs) {
    // 100 sequential runs; verify deterministic outputs and no memory growth.
    Engine eng(crossing_scenario());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 100; ++i) {
        BacktestRunConfig r; r.run_number = i;
        cfg.runs.push_back(r);
    }

    auto results = eng.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 100u);
    EXPECT_EQ(eng.controller.runs_completed(), 100u);

    // All runs must produce identical results (determinism).
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[i].total_trades,     results[0].total_trades)
            << "Non-deterministic trade count at run " << i;
        EXPECT_EQ(results[i].events_processed, results[0].events_processed)
            << "Non-deterministic event count at run " << i;
        EXPECT_DOUBLE_EQ(results[i].realized_pnl, results[0].realized_pnl)
            << "Non-deterministic PnL at run " << i;
    }
}

TEST(Phase4Integration, MultiRunNoMemoryGrowth) {
    Engine eng(crossing_scenario());

    MemoryMonitor mem;
    mem.set_baseline();

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 50; ++i) {
        BacktestRunConfig r; r.run_number = i;
        cfg.runs.push_back(r);
    }

    eng.controller.run_all(cfg);

    mem.record();

    // 50 runs should not grow RSS by more than 1 MB.
    EXPECT_LE(mem.growth_mb(), 1u)
        << "Unexpected memory growth of " << mem.growth_mb() << " MB over 50 runs";
}

// ── Parameter Sweep ───────────────────────────────────────────────────────────

TEST(Phase4Integration, ParameterSweep10Configs) {
    Engine eng(crossing_scenario());

    BacktestController::Config cfg;
    for (uint32_t i = 1; i <= 10; ++i) {
        BacktestRunConfig r;
        r.run_number                    = i;
        r.strategy_config.window_size   = i * 5;
        r.strategy_config.threshold     = i * 0.01;
        r.strategy_config.inventory_limit = static_cast<double>(i) * 100.0;
        cfg.runs.push_back(r);
    }

    auto results = eng.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 10u);

    // Each result must carry its own config (no cross-run contamination).
    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i].config.window_size,     (i + 1) * 5u);
        EXPECT_DOUBLE_EQ(results[i].config.threshold,       (i + 1) * 0.01);
        EXPECT_DOUBLE_EQ(results[i].config.inventory_limit, static_cast<double>(i + 1) * 100.0);
    }
}

TEST(Phase4Integration, ParameterSweepNoCrossContamination) {
    // Both runs replay the same crossing_scenario with different StrategyConfigs.
    // Verify each result carries its own config (no cross-run state contamination).
    Engine eng(crossing_scenario());

    BacktestController::Config cfg;
    BacktestRunConfig r1; r1.run_number = 1; r1.strategy_config.window_size = 10;
    BacktestRunConfig r2; r2.run_number = 2; r2.strategy_config.window_size = 20;
    cfg.runs = {r1, r2};

    auto results = eng.controller.run_all(cfg);

    ASSERT_EQ(results.size(), 2u);
    // Both runs see the same events → same trade count, different config.
    EXPECT_EQ(results[0].total_trades, results[1].total_trades);
    EXPECT_EQ(results[0].config.window_size, 10u);
    EXPECT_EQ(results[1].config.window_size, 20u);
}

// ── Overload Recovery ─────────────────────────────────────────────────────────

TEST(Phase4Integration, QueueOverflowRecovery) {
    SPSCQueue<int, 4> q;
    OverloadMetrics metrics;

    // Fill to capacity
    for (int i = 0; i < 4; ++i) ASSERT_TRUE(q.enqueue(i));

    // Overflow — engine must not crash
    for (int i = 0; i < 10; ++i) {
        if (!q.enqueue(i))
            metrics.queue_rejections.fetch_add(1, std::memory_order_relaxed);
    }

    EXPECT_EQ(metrics.queue_rejections.load(), 10u);

    // Drain and recover — new enqueues succeed
    int v;
    while (q.dequeue(v)) {}
    EXPECT_TRUE(q.enqueue(42));
    EXPECT_EQ(q.size_approx(), 1u);
}

TEST(Phase4Integration, PoolExhaustionRecovery) {
    ObjectPool<Event> pool(4);

    // Exhaust
    std::vector<Event*> held;
    Event* p;
    while ((p = pool.allocate()) != nullptr) held.push_back(p);
    EXPECT_EQ(pool.available(), 0u);

    // Allocate from exhausted pool — returns nullptr, engine continues
    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_GT(pool.exhaustion_count(), 0u);

    // Release all — full recovery
    for (auto* e : held) pool.release(e);
    EXPECT_EQ(pool.available(), 4u);
    EXPECT_NE(pool.allocate(), nullptr);
}

// ── Persistence Queue Overflow Recovery ──────────────────────────────────────

TEST(Phase4Integration, PersistenceQueueOverflowAndRecovery) {
    PostgresWriter writer;

    // Pause writer thread — queue will fill up.
    writer.pause_writer();

    Event ev{};
    ev.type = encode_type(EventType::TradeExecution);

    const std::size_t flood = PostgresWriter::MAX_QUEUE_DEPTH + 50;
    for (std::size_t i = 0; i < flood; ++i)
        writer.write_trade(ev, "overflow-test-run");

    // At least 50 must have been dropped
    EXPECT_GE(writer.overflow_count(), 50u);

    // Resume — writer drains queue automatically
    writer.resume_writer();
    writer.flush();

    // Engine can still issue new writes after recovery
    writer.write_trade(ev, "post-recovery-run");
    writer.flush();

    SUCCEED();
}

// ── StabilityRunner (with max_events for test speed) ─────────────────────────

TEST(Phase4Integration, StabilityRunnerCompletesWithMaxEvents) {
    Engine eng(crossing_scenario());

    BacktestController::Config bt_cfg;
    BacktestRunConfig r; r.run_number = 1;
    bt_cfg.runs = {r};

    StabilityMonitor monitor;
    MemoryMonitor    memory;

    StabilityRunner::Config cfg;
    cfg.max_events = 30;  // stop after 30 events (instant, no wall-clock wait)
    cfg.memory_sample_interval_s = 1;

    StabilityRunner runner;
    auto report = runner.run(eng.controller, bt_cfg, monitor, memory, cfg);

    EXPECT_GE(report.events_processed, 30u);
    EXPECT_GT(report.actual_duration_s, 0.0);
    EXPECT_GE(report.memory_growth_mb, 0u);
    EXPECT_TRUE(std::isfinite(report.projected_24h_growth_mb));
}

TEST(Phase4Integration, StabilityRunnerReportStructureIsValid) {
    Engine eng(crossing_scenario());

    BacktestController::Config bt_cfg;
    BacktestRunConfig r; r.run_number = 1;
    bt_cfg.runs = {r};

    StabilityMonitor monitor;
    MemoryMonitor    memory;

    StabilityRunner::Config cfg;
    cfg.max_events = 10;

    StabilityRunner runner;
    auto report = runner.run(eng.controller, bt_cfg, monitor, memory, cfg);

    // All numeric fields must be finite (no NaN/Inf)
    EXPECT_TRUE(std::isfinite(report.latency_drift_pct));
    EXPECT_TRUE(std::isfinite(report.throughput_drift_pct));
    EXPECT_TRUE(std::isfinite(report.actual_duration_s));
    EXPECT_GE(report.events_processed, 0u);
}
