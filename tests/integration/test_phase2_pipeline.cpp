#include "engine/memory/AllocationGuard.hpp"
#include "engine/memory/ObjectPool.hpp"
#include "engine/memory/StartupAllocator.hpp"
#include "engine/runtime/StartupValidator.hpp"
#include "engine/runtime/SymbolRouter.hpp"
#include "engine/events/Event.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace trading;

// ── Test 1: Full replay correctness ──────────────────────────────────────────

TEST(Phase2Pipeline, FullReplayCorrectness) {
    std::vector<Event> events;
    events.push_back(make_new_order(1000, 1, 101, 10050, 100, Side::Ask));
    events.push_back(make_new_order(1001, 1, 102, 10050,  50, Side::Bid));
    events.push_back(make_new_order(1002, 1, 103, 10050,  50, Side::Bid));

    VectorReplayController rc(events, "phase2_test");

    OrderBook      book(1);
    MatchingEngine matcher;
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());

    std::vector<Event> executions;
    Event event{};
    while (rc.next(event)) {
        if (strategy.process(event) == StrategySignal::Noop) continue;
        if (event_type(event) == EventType::NewOrder) {
            auto out = matcher.process_new_order(book, event);
            for (const auto& e : out.event_span())
                if (event_type(e) == EventType::TradeExecution)
                    executions.push_back(e);
        }
    }

    ASSERT_EQ(executions.size(), 2u);
    EXPECT_EQ(executions[0].price,    10050u);
    EXPECT_EQ(executions[0].quantity, 50u);
    EXPECT_EQ(executions[1].price,    10050u);
    EXPECT_EQ(executions[1].quantity, 50u);
    EXPECT_EQ(rc.events_replayed(), 3u);
}

// ── Test 2: Allocation-free verification ─────────────────────────────────────

TEST(Phase2Pipeline, ZeroUnexpectedAllocationsInReplay) {
    // Pre-allocate everything before the guard activates.
    StartupAllocator alloc;
    StartupAllocator::Config cfg;
    cfg.event_pool_size  = 1024;
    cfg.order_pool_size  = 512;
    cfg.attempt_mlockall = false;
    alloc.start(cfg);

    std::vector<Event> events;
    events.push_back(make_new_order(1000, 1, 101, 10050, 100, Side::Ask));
    events.push_back(make_new_order(1001, 1, 102, 10050,  50, Side::Bid));
    events.reserve(0); // prevent any reallocation inside the guard

    VectorReplayController rc(events, "alloc_test");
    OrderBook      book(1);
    MatchingEngine matcher;
    StrategyEngine strategy(std::make_unique<PassThroughStrategy>());

    uint64_t trade_count = 0;

    {
        AllocationGuard guard;

        Event event{};
        while (rc.next(event)) {
            if (strategy.process(event) == StrategySignal::Noop) continue;
            if (event_type(event) == EventType::NewOrder) {
                auto out = matcher.process_new_order(book, event);
                for (const auto& e : out.event_span())
                    if (event_type(e) == EventType::TradeExecution)
                        ++trade_count;
            }
        }

        EXPECT_EQ(guard.count(), 0u)
            << "Unexpected heap allocations during replay: " << guard.count();
    }

    EXPECT_EQ(trade_count, 1u);
}

// ── Test 3: Pool exhaustion recovery ─────────────────────────────────────────

TEST(Phase2Pipeline, PoolExhaustionDoesNotCrash) {
    ObjectPool<Event> pool(2); // tiny pool — exhausts immediately

    auto* e1 = pool.allocate();
    auto* e2 = pool.allocate();
    EXPECT_NE(e1, nullptr);
    EXPECT_NE(e2, nullptr);

    // Pool exhausted — allocate must return nullptr, not crash
    auto* e3 = pool.allocate();
    EXPECT_EQ(e3, nullptr);
    EXPECT_EQ(pool.exhaustion_count(), 1u);

    // Engine continues — release one slot and reallocate
    pool.release(e1);
    auto* e4 = pool.allocate();
    EXPECT_NE(e4, nullptr);
    EXPECT_EQ(pool.exhaustion_count(), 1u); // counter unchanged

    pool.release(e2);
    pool.release(e4);
    EXPECT_EQ(pool.available(), 2u);
}

TEST(Phase2Pipeline, ExhaustionCountVisibleAfterMultipleExhaustions) {
    ObjectPool<Event> pool(1);
    auto* e = pool.allocate();
    EXPECT_NE(e, nullptr);

    pool.allocate(); // exhausted → count 1
    pool.allocate(); // exhausted → count 2
    pool.allocate(); // exhausted → count 3

    EXPECT_EQ(pool.exhaustion_count(), 3u);
    pool.release(e);
}

// ── Test 4: Multi-symbol routing ─────────────────────────────────────────────

TEST(Phase2Pipeline, MultiSymbolRouting) {
    SymbolRouter router;
    router.register_symbol({1, 10000, 0, 1000}, std::make_unique<PassThroughStrategy>());
    router.register_symbol({2, 20000, 0, 1000}, std::make_unique<PassThroughStrategy>());

    // Resting ask on symbol 1
    router.process(make_new_order(1000, 1, 101, 10050, 100, Side::Ask));
    // Resting ask on symbol 2
    router.process(make_new_order(1001, 2, 201, 20050, 200, Side::Ask));

    // Aggressive bid on symbol 1
    auto out1 = router.process(make_new_order(1002, 1, 102, 10050, 50, Side::Bid));
    // Aggressive bid on symbol 2
    auto out2 = router.process(make_new_order(1003, 2, 202, 20050, 100, Side::Bid));

    auto count_trades = [](const MatchOutput& out) {
        uint64_t n = 0;
        for (const auto& e : out.event_span())
            if (event_type(e) == EventType::TradeExecution) ++n;
        return n;
    };

    EXPECT_EQ(count_trades(out1), 1u);
    EXPECT_EQ(count_trades(out2), 1u);

    // Verify symbol isolation: symbol 1 fill should not affect symbol 2 book
    EXPECT_TRUE(router.book(2)->has_ask()); // symbol 2 still has remainder
}

TEST(Phase2Pipeline, MultiSymbolIsolationNoCrossContamination) {
    SymbolRouter router;
    router.register_symbol({1, 10000, 0, 1000}, std::make_unique<PassThroughStrategy>());
    router.register_symbol({2, 10000, 0, 1000}, std::make_unique<PassThroughStrategy>());

    // Add ask to symbol 1 only
    router.process(make_new_order(1000, 1, 101, 10050, 100, Side::Ask));

    // Aggressive bid on symbol 2 — must NOT trade against symbol 1's ask
    auto out = router.process(make_new_order(1001, 2, 201, 10050, 50, Side::Bid));
    for (const auto& e : out.event_span())
        EXPECT_NE(event_type(e), EventType::TradeExecution);

    // Symbol 1 book unchanged
    EXPECT_TRUE(router.book(1)->has_ask());
    // Symbol 2 bid now resting (no matching ask)
    EXPECT_TRUE(router.book(2)->has_bid());
}
