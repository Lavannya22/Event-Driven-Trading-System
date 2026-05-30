#include "benchmarks/LatencyHistogram.hpp"
#include "benchmarks/PerformanceMetrics.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace trading;
using namespace trading::bench;

// ── LatencyHistogram ──────────────────────────────────────────────────────────

TEST(LatencyHistogram, RecordAndTotalCount) {
    LatencyHistogram h;
    h.record(100);
    h.record(200);
    h.record(300);
    EXPECT_EQ(h.total_count(), 3u);
}

TEST(LatencyHistogram, P50SingleValue) {
    LatencyHistogram h;
    for (int i = 0; i < 100; ++i) h.record(1000);
    // All in the same bucket — p50 should be in the ~512-1023ns bucket
    EXPECT_GT(h.p50(), 0u);
    EXPECT_LE(h.p50(), 2048u);
}

TEST(LatencyHistogram, PercentilesOrdered) {
    LatencyHistogram h;
    // 90 fast + 9 medium + 1 slow
    for (int i = 0; i < 90; ++i)  h.record(100);
    for (int i = 0; i < 9;  ++i)  h.record(10'000);
    h.record(1'000'000);

    EXPECT_LE(h.p50(),  h.p95());
    EXPECT_LE(h.p95(),  h.p99());
    EXPECT_LE(h.p99(),  h.p999());
    EXPECT_LE(h.p999(), h.max_ns());
}

TEST(LatencyHistogram, MaxTracked) {
    LatencyHistogram h;
    h.record(100);
    h.record(50000);
    h.record(1000);
    EXPECT_GE(h.max_ns(), 50000u - 1u);  // within bucket resolution
}

TEST(LatencyHistogram, ResetClearsAll) {
    LatencyHistogram h;
    h.record(500);
    h.record(5000);
    h.reset();
    EXPECT_EQ(h.total_count(), 0u);
    EXPECT_EQ(h.max_ns(), 0u);
    EXPECT_EQ(h.p99(), 0u);
}

TEST(LatencyHistogram, EmptyReturnsZero) {
    LatencyHistogram h;
    EXPECT_EQ(h.p50(), 0u);
    EXPECT_EQ(h.p99(), 0u);
    EXPECT_EQ(h.max_ns(), 0u);
}

TEST(LatencyHistogram, ZeroLatencyHandled) {
    LatencyHistogram h;
    h.record(0);
    EXPECT_EQ(h.total_count(), 1u);
}

TEST(LatencyHistogram, NoAllocationDuringRecord) {
    // ObjectPool pre-allocates; histogram itself is a value type — no heap.
    // Just verify many records don't crash or assert.
    LatencyHistogram h;
    for (uint64_t ns = 1; ns <= 1'000'000; ns *= 2)
        h.record(ns);
    EXPECT_GT(h.total_count(), 0u);
}

TEST(LatencyHistogram, ConcurrentRecordThreadSafe) {
    LatencyHistogram h;
    constexpr int THREADS = 4;
    constexpr int PER_THREAD = 100'000;

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&h, t] {
            for (int i = 0; i < PER_THREAD; ++i)
                h.record(static_cast<uint64_t>((t + 1) * 100 + i % 50));
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(h.total_count(), static_cast<uint64_t>(THREADS * PER_THREAD));
}

// ── PerformanceMetrics ────────────────────────────────────────────────────────

TEST(PerformanceMetrics, SnapshotDefaultsToZero) {
    PerformanceMetrics m;
    auto s = m.snapshot();
    EXPECT_EQ(s.events_processed, 0u);
    EXPECT_EQ(s.p99_ns, 0u);
}

TEST(PerformanceMetrics, RecordLatencyAndSnapshot) {
    PerformanceMetrics m;
    m.latency.record(500);
    m.latency.record(1500);
    m.events_processed.fetch_add(2, std::memory_order_relaxed);
    auto s = m.snapshot();
    EXPECT_EQ(s.events_processed, 2u);
    EXPECT_GT(s.p50_ns, 0u);
}

TEST(PerformanceMetrics, ResetClearsAll) {
    PerformanceMetrics m;
    m.latency.record(1000);
    m.events_processed.store(5, std::memory_order_relaxed);
    m.reset();
    auto s = m.snapshot();
    EXPECT_EQ(s.events_processed, 0u);
    EXPECT_EQ(s.p99_ns, 0u);
}

// ── Shift-based price indexing ────────────────────────────────────────────────

TEST(ShiftIndexing, TickShiftZeroIdentityMapping) {
    // With tick_shift=0, every price maps to a unique index with no gaps.
    OrderBook book(1, 1024, 0);
    // Insert at price 10001 (index = 10001 - 1 = 10000)
    OrderEntry e{1, 10001, 100, 1000, Side::Bid};
    EXPECT_EQ(book.insert(e), OrderBookResult::Ok);
    EXPECT_EQ(book.best_bid(), 10001u);
}

TEST(ShiftIndexing, TickShiftOneHalvesLevels) {
    // tick_shift=1: tick_size=2, price index = (price - 1) >> 1
    // Prices 10000 and 10001 map to the same level (both >> 1 = 4999 or 5000).
    OrderBook book(1, 1024, 1);
    OrderEntry e1{1, 10000, 100, 1000, Side::Ask};
    EXPECT_EQ(book.insert(e1), OrderBookResult::Ok);
    EXPECT_EQ(book.best_ask(), 10000u);
}

TEST(ShiftIndexing, InsertCancelWithShift) {
    OrderBook book(1, 1024, 0);
    OrderEntry e{42, 10050, 200, 1000, Side::Ask};
    EXPECT_EQ(book.insert(e), OrderBookResult::Ok);
    EXPECT_EQ(book.cancel(42), OrderBookResult::Ok);
    EXPECT_FALSE(book.has_ask());
}

TEST(ShiftIndexing, OutOfRangeRejectedWithShift) {
    OrderBook book(1, 1024, 0);
    OrderEntry e{1, 0, 100, 1000, Side::Bid};  // price 0 = out of range
    EXPECT_EQ(book.insert(e), OrderBookResult::PriceOutOfRange);
}
