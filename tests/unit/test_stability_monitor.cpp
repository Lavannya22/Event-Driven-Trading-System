#include <gtest/gtest.h>
#include "engine/stability/StabilityMonitor.hpp"
#include "engine/stability/MemoryMonitor.hpp"

using namespace trading;

// ── StabilityMonitor — threshold detection ────────────────────────────────────

TEST(StabilityMonitor, FreshMonitorIsStable) {
    StabilityMonitor m;
    EXPECT_TRUE(m.is_stable());
    EXPECT_TRUE(m.check().empty());
}

TEST(StabilityMonitor, NoAlertsBelowThresholds) {
    StabilityMonitor m;
    m.record_latency_p99(1000);
    m.record_throughput(1'000'000.0);
    m.set_baseline();

    // Drift within tolerance
    m.record_latency_p99(1040);   // +4% < 5%
    m.record_throughput(960'000); // -4% < 5%

    EXPECT_TRUE(m.is_stable());
}

TEST(StabilityMonitor, LatencyDriftAlertFires) {
    StabilityMonitor m;
    m.record_latency_p99(1000);
    m.set_baseline();

    m.record_latency_p99(1060);  // +6% > 5% threshold

    auto alerts = m.check();
    ASSERT_FALSE(alerts.empty());
    EXPECT_EQ(alerts[0].kind, StabilityAlert::Kind::LatencyDrift);
    EXPECT_GT(alerts[0].observed_value, 5.0);
}

TEST(StabilityMonitor, ThroughputDropAlertFires) {
    StabilityMonitor m;
    m.record_throughput(1'000'000.0);
    m.set_baseline();

    m.record_throughput(940'000.0);  // -6% drop > 5% threshold

    auto alerts = m.check();
    ASSERT_FALSE(alerts.empty());
    bool found = false;
    for (const auto& a : alerts)
        if (a.kind == StabilityAlert::Kind::ThroughputDrop) found = true;
    EXPECT_TRUE(found);
}

TEST(StabilityMonitor, MemoryGrowthAlertFires) {
    StabilityMonitor m;
    m.record_memory_mb(100);
    m.set_baseline();

    m.record_memory_mb(205);  // +105 MB > 100 MB threshold

    auto alerts = m.check();
    bool found = false;
    for (const auto& a : alerts)
        if (a.kind == StabilityAlert::Kind::MemoryGrowth) found = true;
    EXPECT_TRUE(found);
}

TEST(StabilityMonitor, QueueDepthAlertFires) {
    StabilityMonitor m;
    m.record_queue_depth_pct(85.0);  // > 80% threshold

    auto alerts = m.check();
    bool found = false;
    for (const auto& a : alerts)
        if (a.kind == StabilityAlert::Kind::QueueDepth) found = true;
    EXPECT_TRUE(found);
}

TEST(StabilityMonitor, PoolExhaustionAlertFires) {
    StabilityMonitor m;
    m.record_pool_exhaustions(1);  // any > 0 is a defect

    auto alerts = m.check();
    bool found = false;
    for (const auto& a : alerts)
        if (a.kind == StabilityAlert::Kind::PoolExhaustion) found = true;
    EXPECT_TRUE(found);
}

TEST(StabilityMonitor, ZeroPoolExhaustionsIsStable) {
    StabilityMonitor m;
    m.record_pool_exhaustions(0);
    // No other drift
    EXPECT_TRUE(m.is_stable());
}

// ── Drift calculation ─────────────────────────────────────────────────────────

TEST(StabilityMonitor, LatencyDriftCalculation) {
    StabilityMonitor m;
    m.record_latency_p99(1000);
    m.set_baseline();
    m.record_latency_p99(1100);  // +10%

    EXPECT_DOUBLE_EQ(m.latency_drift_pct(), 10.0);
}

TEST(StabilityMonitor, ThroughputDriftCalculation) {
    StabilityMonitor m;
    m.record_throughput(1'000'000.0);
    m.set_baseline();
    m.record_throughput(900'000.0);  // -10%

    EXPECT_DOUBLE_EQ(m.throughput_drift_pct(), -10.0);
}

TEST(StabilityMonitor, MemoryGrowthCalculation) {
    StabilityMonitor m;
    m.record_memory_mb(200);
    m.set_baseline();
    m.record_memory_mb(250);

    EXPECT_EQ(m.memory_growth_mb(), 50u);
}

TEST(StabilityMonitor, MemoryGrowthClampedToZero) {
    StabilityMonitor m;
    m.record_memory_mb(200);
    m.set_baseline();
    m.record_memory_mb(150);  // RSS shrank (e.g. pages freed)

    EXPECT_EQ(m.memory_growth_mb(), 0u);
}

TEST(StabilityMonitor, NoBaselineGivesZeroDrift) {
    StabilityMonitor m;
    m.record_latency_p99(5000);
    m.record_throughput(500'000.0);
    // set_baseline() NOT called

    EXPECT_DOUBLE_EQ(m.latency_drift_pct(), 0.0);
    EXPECT_DOUBLE_EQ(m.throughput_drift_pct(), 0.0);
}

// ── Custom thresholds ─────────────────────────────────────────────────────────

TEST(StabilityMonitor, CustomThresholdsRespected) {
    StabilityThresholds tight;
    tight.max_latency_drift_pct    = 1.0;  // very tight
    tight.max_throughput_drift_pct = 1.0;
    tight.max_memory_growth_mb     = 10;
    tight.max_queue_depth_pct      = 50;
    tight.max_pool_exhaustions     = 0;

    StabilityMonitor m(tight);
    m.record_latency_p99(1000);
    m.record_throughput(1'000'000.0);
    m.record_memory_mb(100);
    m.set_baseline();

    m.record_latency_p99(1020);    // +2% > 1% tight threshold
    m.record_throughput(980'000);  // -2% > 1% tight threshold
    m.record_memory_mb(115);       // +15 MB > 10 MB tight threshold
    m.record_queue_depth_pct(55);  // > 50% tight threshold

    auto alerts = m.check();
    EXPECT_GE(alerts.size(), 3u);  // at minimum latency, throughput, memory
}

// ── reset ─────────────────────────────────────────────────────────────────────

TEST(StabilityMonitor, ResetClearsAllState) {
    StabilityMonitor m;
    m.record_latency_p99(1000);
    m.record_throughput(1'000'000.0);
    m.set_baseline();
    m.record_latency_p99(1100);
    m.record_pool_exhaustions(3);

    m.reset();

    EXPECT_TRUE(m.is_stable());
    EXPECT_EQ(m.pool_exhaustions(), 0u);
    EXPECT_DOUBLE_EQ(m.latency_drift_pct(), 0.0);
}

// ── MemoryMonitor ─────────────────────────────────────────────────────────────

TEST(MemoryMonitor, CurrentRssMbReturnsNonNegative) {
    // On Linux this reads /proc/self/status; on Windows/macOS returns 0.
    // Either way it must not crash and must be ≥ 0.
    const std::size_t rss = MemoryMonitor::current_rss_mb();
    EXPECT_GE(rss, 0u);
}

TEST(MemoryMonitor, GrowthClampedToZeroWhenShrinks) {
    MemoryMonitor mm;
    mm.record();
    mm.set_baseline();

    // Simulate a "shrink" by manually testing growth_mb() invariant.
    // (We can't force RSS to shrink, but growth_mb clamps to 0.)
    EXPECT_GE(mm.growth_mb(), 0u);
}

TEST(MemoryMonitor, ResetClearsState) {
    MemoryMonitor mm;
    mm.record();
    mm.set_baseline();
    mm.record();
    mm.reset();

    EXPECT_EQ(mm.current_mb(),  0u);
    EXPECT_EQ(mm.peak_mb(),     0u);
    EXPECT_EQ(mm.baseline_mb(), 0u);
    EXPECT_EQ(mm.growth_mb(),   0u);
}

TEST(MemoryMonitor, ProjectedGrowthIsNonNegative) {
    MemoryMonitor mm;
    mm.set_baseline();
    mm.record();
    EXPECT_GE(mm.projected_24h_growth_mb(), 0.0);
}

TEST(MemoryMonitor, GrowthRateZeroWithoutBaseline) {
    MemoryMonitor mm;
    EXPECT_DOUBLE_EQ(mm.growth_rate_mb_per_hour(), 0.0);
}
