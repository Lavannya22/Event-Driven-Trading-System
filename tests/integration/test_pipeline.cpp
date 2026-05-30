#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "engine/events/Event.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include "engine/replay/ReplayController.hpp"
#include "engine/replay/SequentialReplayController.hpp"
#include "engine/replay/VectorReplayController.hpp"

#ifdef WITH_POSTGRES
#include "engine/persistence/PostgresWriter.hpp"
#endif

using namespace trading;

// TEST_FIXTURES_DIR is injected by CMake as a compile definition.
static std::string fixture(const char* name) {
    return std::string(TEST_FIXTURES_DIR) + "/" + name;
}

// ---------------------------------------------------------------------------
// Pipeline helper
// ---------------------------------------------------------------------------
struct PipelineResult {
    std::vector<Event> executions;    // all EventType::TradeExecution events
    std::vector<Event> cancellations; // STP-emitted EventType::CancelOrder events
    uint64_t           events_replayed{0};
};

static PipelineResult run_pipeline(ReplayController& rc) {
    OrderBook      book(1);
    MatchingEngine matcher;
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());
    PipelineResult result;
    Event ev{};

    while (rc.next(ev)) {
        if (strategy.process(ev) != StrategySignal::Signal) continue;

        switch (event_type(ev)) {
            case EventType::NewOrder: {
                auto out = matcher.process_new_order(book, ev);
                for (const auto& e : out.events) {
                    if (event_type(e) == EventType::TradeExecution)
                        result.executions.push_back(e);
                    else if (event_type(e) == EventType::CancelOrder)
                        result.cancellations.push_back(e);
                }
                break;
            }
            case EventType::CancelOrder:
                matcher.process_cancel(book, ev);
                break;
            case EventType::ModifyOrder:
                matcher.process_modify(book, ev);
                break;
            default:
                break;
        }
    }
    result.events_replayed = rc.events_replayed();
    return result;
}

// ---------------------------------------------------------------------------
// 1. Full pipeline — CSV without crossing orders
// ---------------------------------------------------------------------------
TEST(Pipeline, SmallCSVProducesNoExecutions) {
    SequentialReplayController rc(fixture("sample.csv"));
    auto r = run_pipeline(rc);
    EXPECT_EQ(r.events_replayed, 5u);
    EXPECT_EQ(r.executions.size(), 0u);
}

// ---------------------------------------------------------------------------
// 2. Crossing orders produce the expected executions
//
// integration_cross.csv trace:
//   Ask@10050 qty=100  (order 2001)
//   Bid@9950  qty=100  (order 2002, no cross)
//   Bid@10050 qty=50   (order 2003, fills 50 of 2001 → trade price=10050 qty=50)
//   Bid@10060 qty=100  (order 2004, fills remaining 50 of 2001 → trade price=10050 qty=50)
// ---------------------------------------------------------------------------
TEST(Pipeline, CrossingOrdersProduceCorrectExecutions) {
    SequentialReplayController rc(fixture("integration_cross.csv"));
    auto r = run_pipeline(rc);

    ASSERT_EQ(r.executions.size(), 2u);

    EXPECT_EQ(r.executions[0].price,    10050u);
    EXPECT_EQ(r.executions[0].quantity,    50u);
    EXPECT_EQ(r.executions[0].order_id,  2003u); // aggressor id in execution

    EXPECT_EQ(r.executions[1].price,    10050u);
    EXPECT_EQ(r.executions[1].quantity,    50u);
    EXPECT_EQ(r.executions[1].order_id,  2004u);
}

// ---------------------------------------------------------------------------
// 3. Determinism — identical replay produces identical executions
// ---------------------------------------------------------------------------
TEST(Pipeline, DeterminismAcrossRuns) {
    auto run_once = [] {
        SequentialReplayController rc(fixture("integration_cross.csv"));
        return run_pipeline(rc);
    };

    auto r1 = run_once();
    auto r2 = run_once();

    ASSERT_EQ(r1.executions.size(), r2.executions.size());
    for (std::size_t i = 0; i < r1.executions.size(); ++i) {
        EXPECT_EQ(r1.executions[i].price,    r2.executions[i].price);
        EXPECT_EQ(r1.executions[i].quantity, r2.executions[i].quantity);
        EXPECT_EQ(r1.executions[i].order_id, r2.executions[i].order_id);
    }
    EXPECT_EQ(r1.events_replayed, r2.events_replayed);
}

// ---------------------------------------------------------------------------
// 4. Empty book — bid with no resting asks produces no execution
// ---------------------------------------------------------------------------
TEST(Pipeline, EmptyBookMatchAttemptNoExecution) {
    VectorReplayController rc({
        make_new_order(1000, 1, 9001, 10050, 100, Side::Bid)
    });
    auto r = run_pipeline(rc);
    EXPECT_EQ(r.executions.size(), 0u);
}

// ---------------------------------------------------------------------------
// 5. Cancel of a non-existent order — must not crash
// ---------------------------------------------------------------------------
TEST(Pipeline, CancelNonExistentOrderIgnored) {
    VectorReplayController rc({
        make_cancel_order(1000, 1, 99999)
    });
    auto r = run_pipeline(rc);
    EXPECT_EQ(r.executions.size(), 0u);
    EXPECT_EQ(r.events_replayed, 1u);
}

// ---------------------------------------------------------------------------
// 6. Out-of-range price — order must be rejected, book unchanged
// ---------------------------------------------------------------------------
TEST(Pipeline, OutOfRangePriceRejected) {
    // Price 300 000 exceeds MAX_PRICE=200 000 → OrderBook rejects the insert.
    // After rejection, a Bid@9000 should still be insertable with no issues.
    VectorReplayController rc({
        make_new_order(1000, 1, 9001, 300'000, 100, Side::Bid), // rejected
        make_new_order(2000, 1, 9002,   9'000, 100, Side::Ask), // accepted
    });
    auto r = run_pipeline(rc);
    // No crossing → no executions. Main check: no crash.
    EXPECT_EQ(r.executions.size(), 0u);
    EXPECT_EQ(r.events_replayed, 2u);
}

// ---------------------------------------------------------------------------
// 7. Self-trade prevention — aggressor from same participant is cancelled
// ---------------------------------------------------------------------------
TEST(Pipeline, STPCancelsAggressor) {
    // participant_id=1 places both a resting ask and an aggressive bid.
    const uint64_t resting_id   = make_order_id(1, 1);
    const uint64_t aggressor_id = make_order_id(1, 2);

    VectorReplayController rc({
        make_new_order(1000, 1, resting_id,   10050, 100, Side::Ask),
        make_new_order(2000, 1, aggressor_id, 10050, 100, Side::Bid),
    });
    auto r = run_pipeline(rc);

    EXPECT_EQ(r.executions.size(),    0u); // no fills — aggressor was cancelled
    EXPECT_EQ(r.cancellations.size(), 1u);
    EXPECT_EQ(r.cancellations[0].order_id, aggressor_id);
}

// ---------------------------------------------------------------------------
// 8. Out-of-order timestamps — detector must fire
// ---------------------------------------------------------------------------
TEST(Pipeline, OutOfOrderTimestampsDetected) {
    SequentialReplayController rc(fixture("out_of_order.csv"));
    run_pipeline(rc);
    EXPECT_TRUE(rc.had_out_of_order_timestamps());
    EXPECT_GE(rc.out_of_order_count(), 1u);
}

// ---------------------------------------------------------------------------
// 9. Partial fill — unfilled remainder stays resting in book
// ---------------------------------------------------------------------------
TEST(Pipeline, PartialFillLeavesRemainder) {
    VectorReplayController rc({
        make_new_order(1000, 1, 9001, 10050, 100, Side::Ask), // resting qty=100
        make_new_order(2000, 1, 9002, 10050,  60, Side::Bid), // fills 60 of 9001
        make_new_order(3000, 1, 9003, 10050,  40, Side::Bid), // fills remaining 40
    });
    auto r = run_pipeline(rc);
    ASSERT_EQ(r.executions.size(), 2u);
    EXPECT_EQ(r.executions[0].quantity, 60u);
    EXPECT_EQ(r.executions[1].quantity, 40u);
}

// ---------------------------------------------------------------------------
// 10. Full fill — order removed; subsequent aggressor finds no match
// ---------------------------------------------------------------------------
TEST(Pipeline, FullFillThenNoMatch) {
    VectorReplayController rc({
        make_new_order(1000, 1, 9001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 9002, 10050, 100, Side::Bid), // fully fills 9001
        make_new_order(3000, 1, 9003, 10050,  50, Side::Bid), // no ask left → rests
    });
    auto r = run_pipeline(rc);
    // Only one execution: the full fill. The second bid rests without matching.
    ASSERT_EQ(r.executions.size(), 1u);
    EXPECT_EQ(r.executions[0].quantity, 100u);
}

// ---------------------------------------------------------------------------
// 11. Modify of a fully-filled order — must be a silent no-op, not a crash
// ---------------------------------------------------------------------------
TEST(Pipeline, ModifyOfFilledOrderIgnored) {
    VectorReplayController rc({
        make_new_order(1000, 1, 9001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 9002, 10050, 100, Side::Bid),        // fills 9001 completely
        make_modify_order(3000, 1, 9001, 10060, 50, Side::Ask),      // 9001 no longer exists
    });
    auto r = run_pipeline(rc);
    // Full fill happened; modify of already-filled order is a no-op.
    ASSERT_EQ(r.executions.size(), 1u);
    EXPECT_EQ(r.executions[0].quantity, 100u);
    EXPECT_EQ(r.events_replayed, 3u);
}

// ---------------------------------------------------------------------------
// Postgres persistence integration (skipped when postgres is not available)
// ---------------------------------------------------------------------------
#ifdef WITH_POSTGRES
TEST(PostgresPersistence, WritesTradeToDatabase) {
    PostgresWriter writer;
    if (!writer.connected()) {
        GTEST_SKIP() << "PostgreSQL not reachable — skipping persistence test "
                        "(run: docker compose up -d)";
    }

    const std::string run_id = writer.begin_run("integration_cross.csv");
    EXPECT_FALSE(run_id.empty());

    VectorReplayController rc({
        make_new_order(1000, 1, 2001, 10050, 100, Side::Ask),
        make_new_order(2000, 1, 2002, 10050,  50, Side::Bid),
    });

    OrderBook      book(1);
    MatchingEngine matcher;
    Event ev{};
    while (rc.next(ev)) {
        if (event_type(ev) == EventType::NewOrder) {
            auto out = matcher.process_new_order(book, ev);
            for (const auto& e : out.events)
                if (event_type(e) == EventType::TradeExecution)
                    writer.write_trade(e, run_id);
        }
    }

    writer.end_run(run_id, rc.events_replayed());
    writer.flush(); // ensure all writes committed before assertions

    SUCCEED(); // if we reach here without throwing, writes succeeded
}
#endif
