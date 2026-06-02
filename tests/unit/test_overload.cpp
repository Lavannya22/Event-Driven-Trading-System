#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "engine/memory/ObjectPool.hpp"
#include "engine/queues/SPSCQueue.hpp"
#include "engine/stability/OverloadMetrics.hpp"
#include "engine/persistence/PostgresWriter.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

// ── Queue Overflow ────────────────────────────────────────────────────────────

TEST(QueueOverflow, EnqueueReturnsFalseWhenFull) {
    SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.enqueue(1));
    EXPECT_TRUE(q.enqueue(2));
    EXPECT_TRUE(q.enqueue(3));
    EXPECT_TRUE(q.enqueue(4));
    EXPECT_FALSE(q.enqueue(5));  // queue full — overflow
}

TEST(QueueOverflow, OverflowCounterIncrements) {
    SPSCQueue<int, 4> q;
    OverloadMetrics metrics;

    for (int i = 0; i < 6; ++i) {
        if (!q.enqueue(i))
            metrics.queue_rejections.fetch_add(1, std::memory_order_relaxed);
    }

    // 4 slots → 2 rejections
    EXPECT_EQ(metrics.queue_rejections.load(), 2u);
}

TEST(QueueOverflow, EngineContinesAfterOverflow) {
    // After overflow, dequeue frees a slot and a new enqueue succeeds.
    SPSCQueue<int, 2> q;
    ASSERT_TRUE(q.enqueue(1));
    ASSERT_TRUE(q.enqueue(2));
    EXPECT_FALSE(q.enqueue(3));  // overflow

    int val;
    ASSERT_TRUE(q.dequeue(val));  // drain one slot
    EXPECT_TRUE(q.enqueue(3));    // now fits
    EXPECT_EQ(q.size_approx(), 2u);
}

TEST(QueueOverflow, AllEnqueuedItemsDequeueableAfterOverflow) {
    SPSCQueue<int, 4> q;
    for (int i = 0; i < 6; ++i) q.enqueue(i);  // 2 will be dropped silently

    std::vector<int> out;
    int v;
    while (q.dequeue(v)) out.push_back(v);

    EXPECT_EQ(out.size(), 4u);  // exactly 4 items made it in
}

// ── Pool Exhaustion ───────────────────────────────────────────────────────────

TEST(PoolExhaustion, ReturnsNullptrWhenExhausted) {
    ObjectPool<Event> pool(2);
    auto* e1 = pool.allocate();
    auto* e2 = pool.allocate();
    EXPECT_NE(e1, nullptr);
    EXPECT_NE(e2, nullptr);

    // Pool exhausted
    auto* e3 = pool.allocate();
    EXPECT_EQ(e3, nullptr);
    EXPECT_EQ(pool.exhaustion_count(), 1u);
}

TEST(PoolExhaustion, EngineContinesAfterExhaustion) {
    ObjectPool<Event> pool(1);
    auto* e = pool.allocate();
    EXPECT_NE(e, nullptr);

    // Multiple exhaustion attempts — engine must not crash
    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.exhaustion_count(), 3u);

    // Release restores availability
    pool.release(e);
    EXPECT_EQ(pool.available(), 1u);
    EXPECT_NE(pool.allocate(), nullptr);
}

TEST(PoolExhaustion, ExhaustionCounterMonotonic) {
    ObjectPool<Event> pool(2);
    pool.allocate(); pool.allocate();

    for (int i = 0; i < 5; ++i) pool.allocate();

    EXPECT_EQ(pool.exhaustion_count(), 5u);
}

// ── Persistence Queue Overflow ────────────────────────────────────────────────

TEST(PersistenceOverflow, QueueBoundedByMaxQueueDepth) {
    // Without PostgreSQL, writes are no-ops — test the overflow counter.
    PostgresWriter writer;

    // Pause the writer thread so the queue fills up.
    writer.pause_writer();

    // Flood the queue beyond MAX_QUEUE_DEPTH.
    const std::size_t flood = PostgresWriter::MAX_QUEUE_DEPTH + 100;
    Event ev{};
    ev.type = encode_type(EventType::TradeExecution);
    for (std::size_t i = 0; i < flood; ++i)
        writer.write_trade(ev, "test-run-id");

    // At least 100 writes must have been dropped.
    EXPECT_GE(writer.overflow_count(), 100u);

    // Resume — writer drains queue, engine continues.
    writer.resume_writer();
    writer.flush();

    // After flush, counter is still ≥ 100 (it is never reset automatically).
    EXPECT_GE(writer.overflow_count(), 100u);
}

TEST(PersistenceOverflow, EngineUnaffectedByOverflow) {
    PostgresWriter writer;
    writer.pause_writer();

    Event ev{};
    ev.type = encode_type(EventType::TradeExecution);

    // Overflow the persistence queue.
    for (std::size_t i = 0; i < PostgresWriter::MAX_QUEUE_DEPTH + 10; ++i)
        writer.write_trade(ev, "run-id");

    // Engine can still enqueue new trades (they get dropped, but no crash).
    EXPECT_NO_THROW(writer.write_trade(ev, "run-id"));

    writer.resume_writer();
    writer.flush();
}

TEST(PersistenceOverflow, RecoveryAfterResume) {
    PostgresWriter writer;

    // Normal operation — queue is not full.
    const uint64_t before = writer.overflow_count();

    Event ev{};
    ev.type = encode_type(EventType::TradeExecution);
    writer.write_trade(ev, "run-id");
    writer.flush();

    // If PostgreSQL is not available, write is a no-op.
    // Either way, overflow_count() must not have increased.
    EXPECT_EQ(writer.overflow_count(), before);
}

// ── OverloadMetrics ───────────────────────────────────────────────────────────

TEST(OverloadMetrics, DefaultsToZero) {
    OverloadMetrics m;
    EXPECT_EQ(m.queue_rejections.load(), 0u);
    EXPECT_EQ(m.persistence_overflows.load(), 0u);
}

TEST(OverloadMetrics, CountersIncrement) {
    OverloadMetrics m;
    m.queue_rejections.fetch_add(3, std::memory_order_relaxed);
    m.persistence_overflows.fetch_add(7, std::memory_order_relaxed);

    auto snap = m.snapshot();
    EXPECT_EQ(snap.queue_rejections,     3u);
    EXPECT_EQ(snap.persistence_overflows,7u);
}

TEST(OverloadMetrics, ResetClearsCounters) {
    OverloadMetrics m;
    m.queue_rejections.fetch_add(10, std::memory_order_relaxed);
    m.persistence_overflows.fetch_add(5, std::memory_order_relaxed);
    m.reset();

    EXPECT_EQ(m.queue_rejections.load(), 0u);
    EXPECT_EQ(m.persistence_overflows.load(), 0u);
}

// ── Database failure isolation ────────────────────────────────────────────────

TEST(DatabaseFailure, EngineRunsWithoutPostgres) {
    // PostgresWriter compiled without -DWITH_POSTGRES → all writes are no-ops.
    // This test verifies the engine doesn't crash when DB is unavailable.
    PostgresWriter writer;  // connection will fail without Docker

    Event ev{};
    ev.type = encode_type(EventType::TradeExecution);
    ev.price = 10050; ev.quantity = 100;

    // These must not throw or crash regardless of connection status.
    const std::string run_id = writer.begin_run("test.csv");
    EXPECT_FALSE(run_id.empty());

    writer.write_trade(ev, run_id);
    writer.end_run(run_id, 1);
    writer.flush();

    SUCCEED();  // if we got here, DB failure was isolated from the engine
}
