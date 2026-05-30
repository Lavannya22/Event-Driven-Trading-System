#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <numeric>
#include "engine/queues/SPSCQueue.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

// ---------------------------------------------------------------------------
// Single-threaded correctness
// ---------------------------------------------------------------------------

TEST(SPSCQueue, FIFOOrdering) {
    SPSCQueue<int, 8> q;
    for (int i = 0; i < 5; ++i) ASSERT_TRUE(q.enqueue(i));

    for (int i = 0; i < 5; ++i) {
        int val = -1;
        ASSERT_TRUE(q.dequeue(val));
        EXPECT_EQ(val, i);
    }
}

TEST(SPSCQueue, EmptyOnConstruction) {
    SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size_approx(), 0u);
}

TEST(SPSCQueue, DequeueFromEmptyReturnsFalse) {
    SPSCQueue<int, 4> q;
    int val = -1;
    EXPECT_FALSE(q.dequeue(val));
    EXPECT_EQ(val, -1);
}

TEST(SPSCQueue, FullDetection) {
    SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.enqueue(1));
    EXPECT_TRUE(q.enqueue(2));
    EXPECT_TRUE(q.enqueue(3));
    EXPECT_TRUE(q.enqueue(4));
    EXPECT_FALSE(q.enqueue(5)); // full
}

TEST(SPSCQueue, CapacityIsCorrect) {
    EXPECT_EQ((SPSCQueue<int, 8>::capacity()), 8u);
    EXPECT_EQ((SPSCQueue<int, 1024>::capacity()), 1024u);
}

TEST(SPSCQueue, EnqueueAfterDequeue) {
    SPSCQueue<int, 4> q;
    ASSERT_TRUE(q.enqueue(1));
    ASSERT_TRUE(q.enqueue(2));
    ASSERT_TRUE(q.enqueue(3));
    ASSERT_TRUE(q.enqueue(4));
    EXPECT_FALSE(q.enqueue(5)); // full

    int val = -1;
    ASSERT_TRUE(q.dequeue(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(q.enqueue(5)); // slot freed
}

TEST(SPSCQueue, WrapAround) {
    SPSCQueue<int, 4> q;
    // Fill and drain twice to exercise wrap-around
    for (int round = 0; round < 2; ++round) {
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(q.enqueue(i + round * 10));
        for (int i = 0; i < 4; ++i) {
            int val = -1;
            ASSERT_TRUE(q.dequeue(val));
            EXPECT_EQ(val, i + round * 10);
        }
    }
}

TEST(SPSCQueue, SizeApprox) {
    SPSCQueue<int, 8> q;
    EXPECT_EQ(q.size_approx(), 0u);
    q.enqueue(1);
    EXPECT_EQ(q.size_approx(), 1u);
    q.enqueue(2);
    EXPECT_EQ(q.size_approx(), 2u);
    int v;
    q.dequeue(v);
    EXPECT_EQ(q.size_approx(), 1u);
}

TEST(SPSCQueue, WorksWithEventType) {
    SPSCQueue<Event, 16> q;
    auto e = make_new_order(1'000'000, 1, 42, 10050, 100, Side::Bid);
    ASSERT_TRUE(q.enqueue(e));

    Event out{};
    ASSERT_TRUE(q.dequeue(out));
    EXPECT_EQ(out.timestamp, e.timestamp);
    EXPECT_EQ(out.order_id,  e.order_id);
    EXPECT_EQ(out.price,     e.price);
    EXPECT_EQ(event_type(out), EventType::NewOrder);
}

// ---------------------------------------------------------------------------
// Multi-threaded producer/consumer (validates ThreadSanitizer compliance)
// ---------------------------------------------------------------------------

TEST(SPSCQueue, ConcurrentProducerConsumer) {
    constexpr int N = 100'000;
    SPSCQueue<int, 1024> q;

    std::vector<int> received;
    received.reserve(N);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!q.enqueue(i)) { /* spin — queue full, wait for consumer */ }
        }
    });

    std::thread consumer([&] {
        int val;
        for (int i = 0; i < N; ++i) {
            while (!q.dequeue(val)) { /* spin — queue empty, wait for producer */ }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(static_cast<int>(received.size()), N);
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(received[i], i) << "FIFO violation at index " << i;
    }
}

TEST(SPSCQueue, ConcurrentEventPipeline) {
    constexpr int N = 10'000;
    SPSCQueue<Event, 512> q;

    std::atomic<uint64_t> total_price{0};

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            auto e = make_new_order(static_cast<uint64_t>(i), 1,
                                    static_cast<uint64_t>(i),
                                    static_cast<uint64_t>(i + 1), 1, Side::Bid);
            while (!q.enqueue(e)) {}
        }
    });

    std::thread consumer([&] {
        Event e{};
        for (int i = 0; i < N; ++i) {
            while (!q.dequeue(e)) {}
            total_price.fetch_add(e.price, std::memory_order_relaxed);
        }
    });

    producer.join();
    consumer.join();

    // sum of (i+1) for i in [0, N) = N*(N+1)/2
    const uint64_t expected = static_cast<uint64_t>(N) * (N + 1) / 2;
    EXPECT_EQ(total_price.load(), expected);
}

// ---------------------------------------------------------------------------
// Phase 2: 10M-operation stress test
// Verifies: no corruption, no lost messages, no deadlocks under sustained load.
// ---------------------------------------------------------------------------

TEST(SPSCQueue, StressTenMillionOps) {
    constexpr uint64_t N = 10'000'000;
    // 4096-slot queue: small enough to force many wrap-arounds, large enough
    // to keep producer and consumer from blocking each other constantly.
    SPSCQueue<uint64_t, 4096> q;

    uint64_t checksum_produced = 0;
    uint64_t checksum_consumed = 0;

    std::thread producer([&] {
        for (uint64_t i = 1; i <= N; ++i) {
            while (!q.enqueue(i)) { /* spin until space available */ }
            checksum_produced += i;
        }
    });

    std::thread consumer([&] {
        uint64_t val;
        uint64_t count = 0;
        while (count < N) {
            if (q.dequeue(val)) {
                checksum_consumed += val;
                ++count;
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify no messages lost and no corruption: both checksums must match.
    EXPECT_EQ(checksum_produced, checksum_consumed);
    // Sanity: sum of 1..N = N*(N+1)/2
    EXPECT_EQ(checksum_consumed, N * (N + 1) / 2);
    // Queue must be empty after all messages are consumed.
    EXPECT_TRUE(q.empty());
}
