#include <gtest/gtest.h>
#include <memory>
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

static Event any_new_order() {
    return make_new_order(0, 1, 42, 10000, 100, Side::Bid);
}

static Event any_cancel() {
    return make_cancel_order(0, 1, 42);
}

static Event any_market_update() {
    return make_market_update(0, 1, 10000, 500);
}

// ---------------------------------------------------------------------------
// PassThroughStrategy
// ---------------------------------------------------------------------------

TEST(PassThroughStrategy, SignalsAllEventTypes) {
    PassThroughStrategy s;
    EXPECT_EQ(s.on_event(any_new_order()),    StrategySignal::Signal);
    EXPECT_EQ(s.on_event(any_cancel()),       StrategySignal::Signal);
    EXPECT_EQ(s.on_event(any_market_update()), StrategySignal::Signal);
    EXPECT_EQ(s.on_event(make_trade_execution(0, 1, 1, 10000, 100)), StrategySignal::Signal);
}

TEST(PassThroughStrategy, Name) {
    PassThroughStrategy s;
    EXPECT_STREQ(s.name(), "PassThrough");
}

TEST(PassThroughStrategy, Deterministic) {
    PassThroughStrategy s;
    auto e = any_new_order();
    // Same event must always produce the same signal
    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(s.on_event(e), StrategySignal::Signal);
}

// ---------------------------------------------------------------------------
// NullStrategy
// ---------------------------------------------------------------------------

TEST(NullStrategy, NoopsAllEventTypes) {
    NullStrategy s;
    EXPECT_EQ(s.on_event(any_new_order()),     StrategySignal::Noop);
    EXPECT_EQ(s.on_event(any_cancel()),        StrategySignal::Noop);
    EXPECT_EQ(s.on_event(any_market_update()), StrategySignal::Noop);
}

TEST(NullStrategy, Name) {
    NullStrategy s;
    EXPECT_STREQ(s.name(), "Null");
}

// ---------------------------------------------------------------------------
// EventTypeFilterStrategy
// ---------------------------------------------------------------------------

TEST(EventTypeFilterStrategy, SignalsAllowedTypes) {
    EventTypeFilterStrategy s({EventType::NewOrder, EventType::CancelOrder});
    EXPECT_EQ(s.on_event(any_new_order()), StrategySignal::Signal);
    EXPECT_EQ(s.on_event(any_cancel()),    StrategySignal::Signal);
}

TEST(EventTypeFilterStrategy, NoopsDisallowedTypes) {
    EventTypeFilterStrategy s({EventType::NewOrder});
    EXPECT_EQ(s.on_event(any_market_update()), StrategySignal::Noop);
    EXPECT_EQ(s.on_event(make_trade_execution(0, 1, 1, 10000, 10)), StrategySignal::Noop);
}

TEST(EventTypeFilterStrategy, EmptyFilterNoopsEverything) {
    EventTypeFilterStrategy s({});
    EXPECT_EQ(s.on_event(any_new_order()),     StrategySignal::Noop);
    EXPECT_EQ(s.on_event(any_cancel()),        StrategySignal::Noop);
    EXPECT_EQ(s.on_event(any_market_update()), StrategySignal::Noop);
}

TEST(EventTypeFilterStrategy, AllTypesAllowed) {
    EventTypeFilterStrategy s({
        EventType::NewOrder, EventType::CancelOrder, EventType::ModifyOrder,
        EventType::MarketUpdate, EventType::TradeExecution
    });
    EXPECT_EQ(s.on_event(any_new_order()),     StrategySignal::Signal);
    EXPECT_EQ(s.on_event(any_market_update()), StrategySignal::Signal);
}

TEST(EventTypeFilterStrategy, Deterministic) {
    EventTypeFilterStrategy s({EventType::NewOrder});
    auto e = any_new_order();
    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(s.on_event(e), StrategySignal::Signal);
}

// ---------------------------------------------------------------------------
// StrategyEngine — counters
// ---------------------------------------------------------------------------

TEST(StrategyEngine, SignalCounterIncrements) {
    StrategyEngine engine(std::make_unique<PassThroughStrategy>());
    engine.process(any_new_order());
    engine.process(any_new_order());
    EXPECT_EQ(engine.signal_count(), 2u);
    EXPECT_EQ(engine.noop_count(),   0u);
}

TEST(StrategyEngine, NoopCounterIncrements) {
    StrategyEngine engine(std::make_unique<NullStrategy>());
    engine.process(any_new_order());
    engine.process(any_new_order());
    engine.process(any_new_order());
    EXPECT_EQ(engine.noop_count(),   3u);
    EXPECT_EQ(engine.signal_count(), 0u);
}

TEST(StrategyEngine, MixedCounters) {
    StrategyEngine engine(std::make_unique<EventTypeFilterStrategy>(
        std::initializer_list<EventType>{EventType::NewOrder}));
    engine.process(any_new_order());      // Signal
    engine.process(any_market_update()); // Noop
    engine.process(any_new_order());      // Signal
    EXPECT_EQ(engine.signal_count(), 2u);
    EXPECT_EQ(engine.noop_count(),   1u);
}

TEST(StrategyEngine, ResetCounters) {
    StrategyEngine engine(std::make_unique<PassThroughStrategy>());
    engine.process(any_new_order());
    engine.process(any_new_order());
    engine.reset_counters();
    EXPECT_EQ(engine.signal_count(), 0u);
    EXPECT_EQ(engine.noop_count(),   0u);
}

// ---------------------------------------------------------------------------
// StrategyEngine — strategy swap
// ---------------------------------------------------------------------------

TEST(StrategyEngine, StrategySwap) {
    StrategyEngine engine(std::make_unique<PassThroughStrategy>());
    EXPECT_EQ(engine.process(any_new_order()), StrategySignal::Signal);

    engine.set_strategy(std::make_unique<NullStrategy>());
    EXPECT_EQ(engine.process(any_new_order()), StrategySignal::Noop);
    EXPECT_STREQ(engine.strategy().name(), "Null");
}

TEST(StrategyEngine, NullStrategyThrowsOnConstruction) {
    EXPECT_THROW(StrategyEngine(nullptr), std::invalid_argument);
}

TEST(StrategyEngine, SetNullStrategyThrows) {
    StrategyEngine engine(std::make_unique<PassThroughStrategy>());
    EXPECT_THROW(engine.set_strategy(nullptr), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Ordering preservation — process returns signal in event order
// ---------------------------------------------------------------------------

TEST(StrategyEngine, PreservesEventOrder) {
    StrategyEngine engine(std::make_unique<EventTypeFilterStrategy>(
        std::initializer_list<EventType>{EventType::NewOrder, EventType::CancelOrder}));

    std::vector<StrategySignal> results;
    results.push_back(engine.process(any_new_order()));      // Signal
    results.push_back(engine.process(any_market_update())); // Noop
    results.push_back(engine.process(any_cancel()));         // Signal
    results.push_back(engine.process(any_market_update())); // Noop

    EXPECT_EQ(results[0], StrategySignal::Signal);
    EXPECT_EQ(results[1], StrategySignal::Noop);
    EXPECT_EQ(results[2], StrategySignal::Signal);
    EXPECT_EQ(results[3], StrategySignal::Noop);
}
