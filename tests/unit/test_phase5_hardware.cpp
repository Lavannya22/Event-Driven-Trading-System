#include <gtest/gtest.h>
#include <cmath>

#include "engine/hardware/HardwareTopologyManager.hpp"
#include "engine/hardware/HugePageAllocator.hpp"
#include "engine/hardware/NumaAllocator.hpp"
#include "engine/hardware/SimdCapabilities.hpp"
#include "engine/transport/TransportLayer.hpp"
#include "benchmarks/AdvancedBenchmarkRunner.hpp"
#include "benchmarks/OptimizationGate.hpp"
#include "engine/backtest/BacktestController.hpp"
#include "engine/backtest/ResultAggregator.hpp"
#include "engine/events/Event.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"

using namespace trading;
using namespace trading::bench;

// ── HardwareTopologyManager ───────────────────────────────────────────────────

TEST(HardwareTopology, DiscoverReturnsValidStruct) {
    auto topo = HardwareTopologyManager::discover();
    // At minimum one logical CPU must exist
    EXPECT_GE(topo.cpu.logical_cores, 1);
    EXPECT_GE(topo.cpu.physical_cores, 1);
    // NUMA has at least one node
    EXPECT_FALSE(topo.numa_nodes.empty());
}

TEST(HardwareTopology, LogicalCoresGEPhysicalCores) {
    auto topo = HardwareTopologyManager::discover();
    EXPECT_GE(topo.cpu.logical_cores, topo.cpu.physical_cores);
}

TEST(HardwareTopology, HyperthreadingFlagConsistent) {
    auto topo = HardwareTopologyManager::discover();
    if (topo.cpu.hyperthreading)
        EXPECT_GT(topo.cpu.logical_cores, topo.cpu.physical_cores);
    else
        EXPECT_LE(topo.cpu.logical_cores, topo.cpu.physical_cores);
}

TEST(HardwareTopology, FormatDoesNotCrash) {
    auto topo = HardwareTopologyManager::discover();
    std::string s = HardwareTopologyManager::format(topo);
    EXPECT_FALSE(s.empty());
}

TEST(HardwareTopology, WarnHyperthreadingDoesNotCrash) {
    auto topo = HardwareTopologyManager::discover();
    EXPECT_NO_THROW(
        HardwareTopologyManager::warn_if_hyperthreading(topo, {0, 1}));
}

// ── HugePageAllocator ─────────────────────────────────────────────────────────

TEST(HugePageAllocator, PageSizeIsNonZero) {
    EXPECT_GT(HugePageAllocator::page_size(), 0u);
}

TEST(HugePageAllocator, AllocateReturnsNonNull) {
    // Whether or not huge pages are available, allocate() must succeed
    // (falls back to malloc on WSL2 / unconfigured systems).
    void* p = HugePageAllocator::allocate(1024);
    ASSERT_NE(p, nullptr);
    HugePageAllocator::deallocate(p, 1024);
}

TEST(HugePageAllocator, AllocateZeroReturnsNonNull) {
    void* p = HugePageAllocator::allocate(0);
    ASSERT_NE(p, nullptr);
    HugePageAllocator::deallocate(p, 0);
}

TEST(HugePageAllocator, AllocateLargeBlockSucceeds) {
    constexpr std::size_t MB = 1024 * 1024;
    void* p = HugePageAllocator::allocate(4 * MB);
    ASSERT_NE(p, nullptr);
    // Write to verify it's actually accessible
    std::memset(p, 0xAB, 4 * MB);
    HugePageAllocator::deallocate(p, 4 * MB);
}

// ── NumaAllocator ─────────────────────────────────────────────────────────────

TEST(NumaAllocator, AllocateNodeMinusOneSucceeds) {
    void* p = NumaAllocator::allocate(256, -1);
    ASSERT_NE(p, nullptr);
    NumaAllocator::deallocate(p, 256);
}

TEST(NumaAllocator, AllocateNode0Succeeds) {
    // On WSL2 / non-NUMA this falls back to malloc — must not crash.
    void* p = NumaAllocator::allocate(256, 0);
    ASSERT_NE(p, nullptr);
    NumaAllocator::deallocate(p, 256);
}

TEST(NumaAllocator, AvailableReturnsBool) {
    // Just verify it doesn't crash; value depends on platform.
    const bool a = NumaAllocator::available();
    (void)a;
    SUCCEED();
}

// ── SimdCapabilities ──────────────────────────────────────────────────────────

TEST(SimdCapabilities, BestReturnsNonEmpty) {
    EXPECT_STRNE(SimdCapabilities::best(), "");
}

TEST(SimdCapabilities, DetectionDoesNotCrash) {
    (void)SimdCapabilities::has_sse42();
    (void)SimdCapabilities::has_avx2();
    (void)SimdCapabilities::has_avx512();
    SUCCEED();
}

TEST(SimdCapabilities, FindFirstNonzeroScalar) {
    uint64_t data[] = {0, 0, 0, 42, 0};
    EXPECT_EQ(find_first_nonzero(data, 5), 3u);
}

TEST(SimdCapabilities, FindFirstNonzeroAllZero) {
    uint64_t data[] = {0, 0, 0, 0};
    EXPECT_EQ(find_first_nonzero(data, 4), 4u);  // returns count
}

TEST(SimdCapabilities, FindFirstNonzeroFirstElement) {
    uint64_t data[] = {7, 0, 0};
    EXPECT_EQ(find_first_nonzero(data, 3), 0u);
}

TEST(SimdCapabilities, FindLastNonzeroScalar) {
    uint64_t data[] = {0, 99, 0, 0, 5};
    EXPECT_EQ(find_last_nonzero(data, 5), 4u);
}

TEST(SimdCapabilities, FindLastNonzeroAllZero) {
    uint64_t data[] = {0, 0, 0};
    EXPECT_EQ(find_last_nonzero(data, 3), 3u);  // returns count
}

TEST(SimdCapabilities, FindFirstAndLastLargeArray) {
    constexpr std::size_t N = 32;
    uint64_t data[N] = {};
    data[5]    = 1;
    data[N - 3] = 1;
    EXPECT_EQ(find_first_nonzero(data, N), 5u);
    EXPECT_EQ(find_last_nonzero(data,  N), N - 3);
}

TEST(SimdCapabilities, SimdMatchesScalarFindFirst) {
    // Verify SIMD and scalar produce identical results on all sizes 1–20.
    for (std::size_t sz = 1; sz <= 20; ++sz) {
        std::vector<uint64_t> data(sz, 0);
        if (sz > 2) data[sz / 2] = 99;

        // Both paths use find_first_nonzero — SIMD is selected automatically.
        std::size_t result = find_first_nonzero(data.data(), sz);
        std::size_t expected = sz;
        for (std::size_t i = 0; i < sz; ++i) {
            if (data[i]) { expected = i; break; }
        }
        EXPECT_EQ(result, expected) << "size=" << sz;
    }
}

// ── TransportLayer ────────────────────────────────────────────────────────────

TEST(TransportLayer, CreateSocketDoesNotCrash) {
    // Socket transport binds to a port — we just verify no throw/crash.
    EXPECT_NO_THROW({
        auto t = TransportLayer::create(TransportLayer::Mode::Socket,
                                        "127.0.0.1", 19999);
        EXPECT_NE(t, nullptr);
        EXPECT_STREQ(t->mode_name(), "socket");
    });
}

TEST(TransportLayer, DpdkFallsBackToSocket) {
    // On WSL2/non-DPDK this must fall back to socket without crashing.
    auto t = TransportLayer::create(TransportLayer::Mode::DPDK,
                                    "127.0.0.1", 19998);
    ASSERT_NE(t, nullptr);
    EXPECT_STREQ(t->mode_name(), "socket");
}

// ── BacktestController latency histogram (Phase 5 gap closure) ───────────────

TEST(BacktestControllerP5, LatencyHistogramPopulatedAfterRun) {
    std::vector<Event> events = {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 1002, 10050,  50, Side::Bid),
    };
    VectorReplayController rc(std::move(events));
    OrderBook book(1);
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());
    MatchingEngine matcher;
    ResultAggregator aggregator;
    BacktestController controller(rc, book, strategy, matcher, aggregator);

    BacktestRunConfig cfg; cfg.run_number = 1;
    controller.run_single(cfg);

    const auto& hist = controller.latency_histogram();
    EXPECT_GT(hist.total_count(), 0u);
    EXPECT_GT(hist.p50(), 0u);
    EXPECT_TRUE(std::isfinite(static_cast<double>(hist.max_ns())));
}

TEST(BacktestControllerP5, LatencyHistogramResetBetweenRuns) {
    std::vector<Event> events = {
        make_new_order(1000, 1, 1001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 1002, 10050,  50, Side::Bid),
    };
    VectorReplayController rc(std::move(events));
    OrderBook book(1);
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());
    MatchingEngine matcher;
    ResultAggregator aggregator;
    BacktestController controller(rc, book, strategy, matcher, aggregator);

    BacktestController::Config cfg;
    BacktestRunConfig r1; r1.run_number = 1;
    BacktestRunConfig r2; r2.run_number = 2;
    cfg.runs = {r1, r2};

    auto results = controller.run_all(cfg);
    ASSERT_EQ(results.size(), 2u);

    // After run_all, histogram reflects the last run only.
    EXPECT_GT(controller.latency_histogram().total_count(), 0u);
}

TEST(BacktestControllerP5, ReplayModeUnlimitedFastest) {
    std::vector<Event> events;
    for (uint64_t i = 0; i < 10; ++i)
        events.push_back(make_new_order(i * 1'000'000'000ull, 1,
                                         i + 1, 10050, 10, Side::Ask));

    VectorReplayController rc(std::move(events));
    OrderBook book(1);
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());
    MatchingEngine matcher;
    ResultAggregator aggregator;
    BacktestController controller(rc, book, strategy, matcher, aggregator);

    BacktestRunConfig cfg; cfg.run_number = 1;
    // Unlimited mode — events 1 second apart in timestamp but should complete
    // in microseconds wall-clock.
    auto start = std::chrono::steady_clock::now();
    controller.run_single(cfg, ReplayMode::Unlimited);
    auto end = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();

    EXPECT_LT(ms, 100.0)  // must complete well under 100ms
        << "Unlimited mode took " << ms << "ms for 10 events — too slow";
}

// ── AdvancedBenchmarkRunner ───────────────────────────────────────────────────

TEST(AdvancedBenchmarkRunner, CategoryNameNotEmpty) {
    EXPECT_STRNE(category_name(OptCategory::Baseline), "");
    EXPECT_STRNE(category_name(OptCategory::SIMD), "");
    EXPECT_STRNE(category_name(OptCategory::FullOptimization), "");
}

// ── OptimizationGate ──────────────────────────────────────────────────────────

TEST(OptimizationGate, PassesWhenThroughputImproves10Pct) {
    RepStats base;  base.mean_throughput = 10e6; base.mean_p99_ns = 5000;
    RepStats cand;  cand.mean_throughput = 11e6; cand.mean_p99_ns = 5000;  // +10%

    auto r = OptimizationGate::evaluate(OptCategory::SIMD, base, cand);
    EXPECT_TRUE(r.passed);
    EXPECT_NEAR(r.throughput_gain_pct, 10.0, 0.01);
}

TEST(OptimizationGate, PassesWhenLatencyImproves10Pct) {
    RepStats base;  base.mean_throughput = 10e6; base.mean_p99_ns = 5000;
    RepStats cand;  cand.mean_throughput = 10e6; cand.mean_p99_ns = 4500;  // -10% p99

    auto r = OptimizationGate::evaluate(OptCategory::NUMA, base, cand);
    EXPECT_TRUE(r.passed);
    EXPECT_NEAR(r.latency_gain_pct, 10.0, 0.01);
}

TEST(OptimizationGate, FailsWhenImprovementBelow10Pct) {
    RepStats base;  base.mean_throughput = 10e6; base.mean_p99_ns = 5000;
    RepStats cand;  cand.mean_throughput = 10.5e6; cand.mean_p99_ns = 4900;  // +5% / +2%

    auto r = OptimizationGate::evaluate(OptCategory::HugePages, base, cand);
    EXPECT_FALSE(r.passed);
}

TEST(OptimizationGate, FailsOnRegression) {
    RepStats base;  base.mean_throughput = 10e6; base.mean_p99_ns = 5000;
    RepStats cand;  cand.mean_throughput = 9e6;  cand.mean_p99_ns = 5500;  // worse

    auto r = OptimizationGate::evaluate(OptCategory::DPDK, base, cand);
    EXPECT_FALSE(r.passed);
    EXPECT_LT(r.throughput_gain_pct, 0.0);
    EXPECT_LT(r.latency_gain_pct,    0.0);
}

TEST(OptimizationGate, ExactlyAtThresholdPasses) {
    RepStats base;  base.mean_throughput = 10e6; base.mean_p99_ns = 5000;
    RepStats cand;  cand.mean_throughput = 11e6; cand.mean_p99_ns = 5000;  // exactly 10%

    auto r = OptimizationGate::evaluate(OptCategory::SIMD, base, cand);
    EXPECT_TRUE(r.passed);
}
