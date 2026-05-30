#include <gtest/gtest.h>
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/events/Event.hpp"

using namespace trading;

// Participant IDs used across tests
static constexpr uint16_t P1 = 1;  // aggressor in most tests
static constexpr uint16_t P2 = 2;  // resting orders
static constexpr uint16_t P0 = 0;  // anonymous — STP disabled

// Build a NewOrder event from constituent parts.
static Event new_order(uint16_t participant, uint64_t seq,
                        uint64_t price, uint64_t qty, Side side,
                        uint32_t sym = 1, uint64_t ts = 0) {
    return make_new_order(ts, sym, make_order_id(participant, seq),
                           price, qty, side);
}

// Pre-populate the book with a resting order and return its id.
static uint64_t rest(OrderBook& book, MatchingEngine& me,
                      uint16_t participant, uint64_t seq,
                      uint64_t price, uint64_t qty, Side side) {
    Event e = new_order(participant, seq, price, qty, side);
    me.process_new_order(book, e);
    return make_order_id(participant, seq);
}

// ---------------------------------------------------------------------------
// Empty book — aggressor always becomes resting
// ---------------------------------------------------------------------------

TEST(MatchingEngine, EmptyBookBidBecomesResting) {
    OrderBook book(1); MatchingEngine me;
    auto out = me.process_new_order(book, new_order(P1, 1, 10000, 100, Side::Bid));
    EXPECT_EQ(out.result, MatchResult::Ok);
    EXPECT_TRUE(out.events.empty());
    EXPECT_TRUE(book.has_bid());
    EXPECT_EQ(book.best_bid(), 10000u);
}

TEST(MatchingEngine, EmptyBookAskBecomesResting) {
    OrderBook book(1); MatchingEngine me;
    auto out = me.process_new_order(book, new_order(P1, 1, 10050, 50, Side::Ask));
    EXPECT_EQ(out.result, MatchResult::Ok);
    EXPECT_TRUE(out.events.empty());
    EXPECT_TRUE(book.has_ask());
    EXPECT_EQ(book.best_ask(), 10050u);
}

// ---------------------------------------------------------------------------
// No cross — aggressor becomes resting
// ---------------------------------------------------------------------------

TEST(MatchingEngine, BidBelowBestAskBecomesResting) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 100, Side::Ask);
    auto out = me.process_new_order(book, new_order(P1, 2, 9999, 100, Side::Bid));
    EXPECT_EQ(out.result, MatchResult::Ok);
    EXPECT_TRUE(out.events.empty());
    EXPECT_EQ(book.best_bid(), 9999u);
    EXPECT_EQ(book.best_ask(), 10050u);
}

TEST(MatchingEngine, AskAboveBestBidBecomesResting) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10000, 100, Side::Bid);
    auto out = me.process_new_order(book, new_order(P1, 2, 10051, 50, Side::Ask));
    EXPECT_EQ(out.result, MatchResult::Ok);
    EXPECT_TRUE(out.events.empty());
    EXPECT_EQ(book.best_bid(), 10000u);
    EXPECT_EQ(book.best_ask(), 10051u);
}

// ---------------------------------------------------------------------------
// Full fill
// ---------------------------------------------------------------------------

TEST(MatchingEngine, FullFillBidAggressor) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 100, Side::Ask);

    auto out = me.process_new_order(book, new_order(P1, 2, 10050, 100, Side::Bid));

    ASSERT_EQ(out.result, MatchResult::Ok);
    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(event_type(out.events[0]), EventType::TradeExecution);
    EXPECT_EQ(out.events[0].price,    10050u);
    EXPECT_EQ(out.events[0].quantity, 100u);

    EXPECT_FALSE(book.has_ask());
    EXPECT_FALSE(book.has_bid());  // aggressor fully filled, not resting
}

TEST(MatchingEngine, FullFillAskAggressor) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10000, 100, Side::Bid);

    auto out = me.process_new_order(book, new_order(P1, 2, 10000, 100, Side::Ask));

    ASSERT_EQ(out.result, MatchResult::Ok);
    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(event_type(out.events[0]), EventType::TradeExecution);
    EXPECT_EQ(out.events[0].price,    10000u);
    EXPECT_EQ(out.events[0].quantity, 100u);
    EXPECT_FALSE(book.has_bid());
    EXPECT_FALSE(book.has_ask());
}

// Fill price is the resting order's price, not the aggressor's limit.
TEST(MatchingEngine, FillPriceIsRestingPrice) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 100, Side::Ask);

    // Aggressor bid at 10100 — crosses the 10050 ask.
    auto out = me.process_new_order(book, new_order(P1, 2, 10100, 100, Side::Bid));

    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(out.events[0].price, 10050u);  // passive (resting) price
}

// ---------------------------------------------------------------------------
// Partial fills
// ---------------------------------------------------------------------------

TEST(MatchingEngine, AggressorPartialFillRemainsResting) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 60, Side::Ask);  // resting ask qty=60

    // Aggressor bid qty=100, only 60 available
    auto out = me.process_new_order(book, new_order(P1, 2, 10050, 100, Side::Bid));

    ASSERT_EQ(out.result, MatchResult::Ok);
    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(out.events[0].quantity, 60u);  // only 60 filled

    // Remaining 40 of aggressor should be resting
    EXPECT_TRUE(book.has_bid());
    EXPECT_EQ(book.quantity_at(10050, Side::Bid), 40u);
    EXPECT_FALSE(book.has_ask());
}

TEST(MatchingEngine, RestingPartialFillRemainsInBook) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 200, Side::Ask);  // resting ask qty=200

    // Aggressor bid qty=50
    auto out = me.process_new_order(book, new_order(P1, 2, 10050, 50, Side::Bid));

    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(out.events[0].quantity, 50u);
    EXPECT_EQ(book.quantity_at(10050, Side::Ask), 150u);  // 200 - 50
    EXPECT_FALSE(book.has_bid());
}

// ---------------------------------------------------------------------------
// Multiple fills — price-time priority across levels
// ---------------------------------------------------------------------------

TEST(MatchingEngine, BidClearsMultiplePriceLevels) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 50, Side::Ask);
    rest(book, me, P2, 2, 10060, 50, Side::Ask);

    // Aggressor bid at 10060 clears both levels
    auto out = me.process_new_order(book, new_order(P1, 3, 10060, 100, Side::Bid));

    ASSERT_EQ(out.result, MatchResult::Ok);
    ASSERT_EQ(out.events.size(), 2u);
    EXPECT_EQ(out.events[0].price, 10050u);  // best ask filled first
    EXPECT_EQ(out.events[1].price, 10060u);
    EXPECT_FALSE(book.has_ask());
    EXPECT_FALSE(book.has_bid());
}

TEST(MatchingEngine, FIFOWithinSameLevel) {
    OrderBook book(1); MatchingEngine me;
    uint64_t id1 = rest(book, me, P2, 1, 10050, 40, Side::Ask);
    uint64_t id2 = rest(book, me, P2, 2, 10050, 40, Side::Ask);
    (void)id1; (void)id2;

    // Aggressor fills only the first order fully and second partially
    auto out = me.process_new_order(book, new_order(P1, 3, 10050, 50, Side::Bid));

    ASSERT_EQ(out.events.size(), 2u);
    EXPECT_EQ(out.events[0].quantity, 40u);  // first resting order filled first
    EXPECT_EQ(out.events[1].quantity, 10u);  // second resting order partially filled
    EXPECT_EQ(book.quantity_at(10050, Side::Ask), 30u);  // 40-10 remains
}

// ---------------------------------------------------------------------------
// Self-Trade Prevention (STP)
// ---------------------------------------------------------------------------

TEST(MatchingEngine, STPCancelsAggressor) {
    OrderBook book(1); MatchingEngine me;
    // P1 places a resting ask
    rest(book, me, P1, 1, 10050, 100, Side::Ask);

    // P1 now sends a bid that would cross its own ask
    auto out = me.process_new_order(book, new_order(P1, 2, 10050, 100, Side::Bid));

    EXPECT_EQ(out.result, MatchResult::STPTriggered);
    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(event_type(out.events[0]), EventType::CancelOrder);
    EXPECT_EQ(out.events[0].order_id, make_order_id(P1, 2));  // aggressor cancelled

    // Resting ask must still be in the book — not consumed
    EXPECT_TRUE(book.has_ask());
    EXPECT_EQ(book.quantity_at(10050, Side::Ask), 100u);
    // Aggressor must NOT be in the book
    EXPECT_FALSE(book.has_bid());
}

TEST(MatchingEngine, STPAfterPartialFillsCancelsRemainder) {
    OrderBook book(1); MatchingEngine me;
    // P2 order first, then P1 order at same level
    rest(book, me, P2, 1, 10050, 50, Side::Ask);   // matched first (FIFO)
    rest(book, me, P1, 2, 10050, 100, Side::Ask);  // STP target

    // P1 aggressor bid qty=200: fills P2's 50, then hits own P1 order → STP
    auto out = me.process_new_order(book, new_order(P1, 3, 10050, 200, Side::Bid));

    EXPECT_EQ(out.result, MatchResult::STPTriggered);
    // First event: fill of 50 against P2
    ASSERT_GE(out.events.size(), 2u);
    EXPECT_EQ(event_type(out.events[0]), EventType::TradeExecution);
    EXPECT_EQ(out.events[0].quantity, 50u);
    // Last event: STP cancel of aggressor
    EXPECT_EQ(event_type(out.events.back()), EventType::CancelOrder);

    // P1 resting ask still in book (not consumed)
    EXPECT_EQ(book.quantity_at(10050, Side::Ask), 100u);
    // Aggressor remainder NOT resting
    EXPECT_FALSE(book.has_bid());
}

TEST(MatchingEngine, AnonymousOrdersNoSTP) {
    OrderBook book(1); MatchingEngine me;
    // Two anonymous orders (participant 0) — STP must not trigger
    rest(book, me, P0, 1, 10050, 100, Side::Ask);
    auto out = me.process_new_order(book, new_order(P0, 2, 10050, 100, Side::Bid));

    EXPECT_EQ(out.result, MatchResult::Ok);
    ASSERT_EQ(out.events.size(), 1u);
    EXPECT_EQ(event_type(out.events[0]), EventType::TradeExecution);
}

TEST(MatchingEngine, DifferentParticipantsNoSTP) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 100, Side::Ask);
    auto out = me.process_new_order(book, new_order(P1, 2, 10050, 100, Side::Bid));

    EXPECT_EQ(out.result, MatchResult::Ok);
    EXPECT_EQ(out.events.size(), 1u);
    EXPECT_EQ(event_type(out.events[0]), EventType::TradeExecution);
}

// ---------------------------------------------------------------------------
// process_cancel
// ---------------------------------------------------------------------------

TEST(MatchingEngine, ProcessCancelRemovesRestingOrder) {
    OrderBook book(1); MatchingEngine me;
    uint64_t id = rest(book, me, P2, 1, 10050, 100, Side::Ask);

    Event cancel_ev = make_cancel_order(0, 1, id);
    auto result = me.process_cancel(book, cancel_ev);

    EXPECT_EQ(result, OrderBookResult::Ok);
    EXPECT_FALSE(book.has_ask());
}

TEST(MatchingEngine, ProcessCancelNonExistentOrder) {
    OrderBook book(1); MatchingEngine me;
    Event cancel_ev = make_cancel_order(0, 1, make_order_id(P1, 999));
    EXPECT_EQ(me.process_cancel(book, cancel_ev), OrderBookResult::OrderNotFound);
}

// ---------------------------------------------------------------------------
// process_modify
// ---------------------------------------------------------------------------

TEST(MatchingEngine, ProcessModifyChangesPrice) {
    OrderBook book(1); MatchingEngine me;
    uint64_t id = rest(book, me, P2, 1, 10050, 100, Side::Ask);

    Event mod_ev = make_modify_order(0, 1, id, 10060, 100, Side::Ask);
    EXPECT_EQ(me.process_modify(book, mod_ev), OrderBookResult::Ok);
    EXPECT_EQ(book.best_ask(), 10060u);
}

// ---------------------------------------------------------------------------
// Execution event fields
// ---------------------------------------------------------------------------

TEST(MatchingEngine, ExecutionEventHasCorrectFields) {
    OrderBook book(1); MatchingEngine me;
    rest(book, me, P2, 1, 10050, 100, Side::Ask);

    uint64_t agg_id = make_order_id(P1, 2);
    Event agg = make_new_order(9999, 1, agg_id, 10050, 100, Side::Bid);
    auto out = me.process_new_order(book, agg);

    ASSERT_EQ(out.events.size(), 1u);
    const Event& exec = out.events[0];
    EXPECT_EQ(event_type(exec), EventType::TradeExecution);
    EXPECT_EQ(exec.symbol_id, 1u);
    EXPECT_EQ(exec.order_id,  agg_id);
    EXPECT_EQ(exec.price,     10050u);
    EXPECT_EQ(exec.quantity,  100u);
    EXPECT_EQ(exec.timestamp, 9999u);
}
