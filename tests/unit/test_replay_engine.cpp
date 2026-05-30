#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "engine/replay/ReplayController.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/replay/SequentialReplayController.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

// TEST_FIXTURES_DIR is injected by CMake as a compile definition.
static std::string fixture(const char* name) {
    return std::string(TEST_FIXTURES_DIR) + "/" + name;
}

// ---------------------------------------------------------------------------
// VectorReplayController
// ---------------------------------------------------------------------------

TEST(VectorReplay, EmptyVectorIsDoneImmediately) {
    VectorReplayController rc({});
    Event e{};
    EXPECT_FALSE(rc.next(e));
    EXPECT_TRUE(rc.done());
    EXPECT_EQ(rc.events_replayed(), 0u);
}

TEST(VectorReplay, YieldsEventsInOrder) {
    auto e1 = make_new_order(1000, 1, 1, 10050, 100, Side::Ask);
    auto e2 = make_cancel_order(2000, 1, 1);
    VectorReplayController rc({e1, e2});

    Event out{};
    ASSERT_TRUE(rc.next(out));
    EXPECT_EQ(out.timestamp, 1000u);
    EXPECT_EQ(event_type(out), EventType::NewOrder);

    ASSERT_TRUE(rc.next(out));
    EXPECT_EQ(out.timestamp, 2000u);
    EXPECT_EQ(event_type(out), EventType::CancelOrder);

    EXPECT_FALSE(rc.next(out));
    EXPECT_TRUE(rc.done());
    EXPECT_EQ(rc.events_replayed(), 2u);
}

TEST(VectorReplay, ResetRestartsDeterministically) {
    auto e = make_new_order(1000, 1, 42, 10050, 100, Side::Bid);
    VectorReplayController rc({e, e, e});

    Event out{};
    rc.next(out); rc.next(out); rc.next(out);
    EXPECT_EQ(rc.events_replayed(), 3u);
    EXPECT_TRUE(rc.done());

    rc.reset();
    EXPECT_FALSE(rc.done());
    EXPECT_EQ(rc.events_replayed(), 0u);

    std::vector<Event> run1, run2;
    VectorReplayController rc2({e, e, e});
    while (rc2.next(out)) run1.push_back(out);
    rc2.reset();
    while (rc2.next(out)) run2.push_back(out);

    ASSERT_EQ(run1.size(), run2.size());
    for (std::size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i].timestamp, run2[i].timestamp);
        EXPECT_EQ(run1[i].type,      run2[i].type);
    }
}

TEST(VectorReplay, SourceName) {
    VectorReplayController rc({}, "my-test");
    EXPECT_STREQ(rc.source_name(), "my-test");
}

TEST(VectorReplay, PreservesAllEventFields) {
    auto e = make_new_order(999, 7, 42, 10050, 200, Side::Ask);
    VectorReplayController rc({e});
    Event out{};
    ASSERT_TRUE(rc.next(out));
    EXPECT_EQ(out.timestamp, 999u);
    EXPECT_EQ(out.symbol_id, 7u);
    EXPECT_EQ(out.order_id,  42u);
    EXPECT_EQ(out.price,     10050u);
    EXPECT_EQ(out.quantity,  200u);
    EXPECT_EQ(event_side(out), Side::Ask);
    EXPECT_EQ(event_type(out), EventType::NewOrder);
}

// ---------------------------------------------------------------------------
// SequentialReplayController — CSV parsing
// ---------------------------------------------------------------------------

TEST(SequentialReplay, ReadsSampleFixture) {
    SequentialReplayController rc(fixture("sample.csv"));
    EXPECT_FALSE(rc.done());

    Event out{};
    uint64_t count = 0;
    while (rc.next(out)) ++count;

    EXPECT_EQ(count, 5u);  // 5 data rows in sample.csv
    EXPECT_EQ(rc.events_replayed(), 5u);
    EXPECT_TRUE(rc.done());
}

TEST(SequentialReplay, ParsedFieldsAreCorrect) {
    SequentialReplayController rc(fixture("sample.csv"));
    Event out{};

    // First row: 1000000,1,1,10050,100,1001,1  (NewOrder Ask)
    ASSERT_TRUE(rc.next(out));
    EXPECT_EQ(out.timestamp, 1000000u);
    EXPECT_EQ(out.symbol_id, 1u);
    EXPECT_EQ(out.price,     10050u);
    EXPECT_EQ(out.quantity,  100u);
    EXPECT_EQ(out.order_id,  1001u);
    EXPECT_EQ(event_type(out), EventType::NewOrder);
    EXPECT_EQ(event_side(out), Side::Ask);

    // Second row: 1000100,1,1,9950,100,1002,0  (NewOrder Bid)
    ASSERT_TRUE(rc.next(out));
    EXPECT_EQ(out.timestamp, 1000100u);
    EXPECT_EQ(event_side(out), Side::Bid);
}

TEST(SequentialReplay, EmptyFileIsDoneImmediately) {
    SequentialReplayController rc(fixture("empty.csv"));
    Event out{};
    EXPECT_FALSE(rc.next(out));
    EXPECT_TRUE(rc.done());
    EXPECT_EQ(rc.events_replayed(), 0u);
}

TEST(SequentialReplay, MissingFileThrows) {
    EXPECT_THROW(
        SequentialReplayController rc(fixture("nonexistent_file.csv")),
        std::runtime_error);
}

TEST(SequentialReplay, ResetProducesSameEventsAsFreshRun) {
    SequentialReplayController rc(fixture("sample.csv"));

    auto drain = [&]() {
        std::vector<Event> events;
        Event out{};
        while (rc.next(out)) events.push_back(out);
        return events;
    };

    auto run1 = drain();
    rc.reset();
    auto run2 = drain();

    ASSERT_EQ(run1.size(), run2.size());
    for (std::size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i].timestamp, run2[i].timestamp);
        EXPECT_EQ(run1[i].type,      run2[i].type);
        EXPECT_EQ(run1[i].order_id,  run2[i].order_id);
    }
}

// ---------------------------------------------------------------------------
// Out-of-order timestamp detection (spec required edge case)
// ---------------------------------------------------------------------------

TEST(SequentialReplay, DetectsOutOfOrderTimestamps) {
    SequentialReplayController rc(fixture("out_of_order.csv"));
    Event out{};
    while (rc.next(out)) {}

    EXPECT_TRUE(rc.had_out_of_order_timestamps());
    EXPECT_EQ(rc.out_of_order_count(), 1u);  // exactly one out-of-order row
}

TEST(SequentialReplay, InOrderFileHasNoOutOfOrderFlag) {
    SequentialReplayController rc(fixture("sample.csv"));
    Event out{};
    while (rc.next(out)) {}
    EXPECT_FALSE(rc.had_out_of_order_timestamps());
}

// ---------------------------------------------------------------------------
// Polymorphic use via ReplayController interface
// ---------------------------------------------------------------------------

TEST(ReplayController, PolymorphicVectorUsage) {
    auto e = make_new_order(500, 1, 1, 10000, 50, Side::Bid);
    std::unique_ptr<ReplayController> rc =
        std::make_unique<VectorReplayController>(std::vector<Event>{e, e});

    Event out{};
    EXPECT_TRUE(rc->next(out));
    EXPECT_TRUE(rc->next(out));
    EXPECT_FALSE(rc->next(out));
    EXPECT_EQ(rc->events_replayed(), 2u);
}

TEST(ReplayController, PolymorphicCSVUsage) {
    std::unique_ptr<ReplayController> rc =
        std::make_unique<SequentialReplayController>(fixture("sample.csv"));

    Event out{};
    uint64_t n = 0;
    while (rc->next(out)) ++n;
    EXPECT_EQ(n, rc->events_replayed());
    EXPECT_GT(n, 0u);
}
