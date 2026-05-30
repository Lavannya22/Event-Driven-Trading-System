#include <gtest/gtest.h>
#include "engine/orderbook/OrderBook.hpp"

using namespace trading;

// Helper: build an OrderEntry quickly
static OrderEntry make_entry(uint64_t id, uint64_t price, uint64_t qty,
                              Side side, uint64_t ts = 0) {
    return OrderEntry{id, price, qty, ts, side};
}

// ---------------------------------------------------------------------------
// Insert correctness
// ---------------------------------------------------------------------------

TEST(OrderBook, InsertBidUpdatesState) {
    OrderBook book(1);
    ASSERT_EQ(book.insert(make_entry(1, 10000, 100, Side::Bid)), OrderBookResult::Ok);
    EXPECT_TRUE(book.has_bid());
    EXPECT_EQ(book.best_bid(), 10000u);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 100u);
}

TEST(OrderBook, InsertAskUpdatesState) {
    OrderBook book(1);
    ASSERT_EQ(book.insert(make_entry(1, 10050, 50, Side::Ask)), OrderBookResult::Ok);
    EXPECT_TRUE(book.has_ask());
    EXPECT_EQ(book.best_ask(), 10050u);
    EXPECT_EQ(book.quantity_at(10050, Side::Ask), 50u);
}

TEST(OrderBook, EmptyBookHasNoBidOrAsk) {
    OrderBook book(1);
    EXPECT_FALSE(book.has_bid());
    EXPECT_FALSE(book.has_ask());
    EXPECT_EQ(book.spread(), 0u);
}

TEST(OrderBook, BestBidTracksHighestBid) {
    OrderBook book(1);
    book.insert(make_entry(1, 9900, 100, Side::Bid));
    book.insert(make_entry(2, 10000, 100, Side::Bid));
    book.insert(make_entry(3, 9950, 100, Side::Bid));
    EXPECT_EQ(book.best_bid(), 10000u);
}

TEST(OrderBook, BestAskTracksLowestAsk) {
    OrderBook book(1);
    book.insert(make_entry(1, 10100, 50, Side::Ask));
    book.insert(make_entry(2, 10050, 50, Side::Ask));
    book.insert(make_entry(3, 10200, 50, Side::Ask));
    EXPECT_EQ(book.best_ask(), 10050u);
}

TEST(OrderBook, SpreadIsCorrect) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    book.insert(make_entry(2, 10050, 50,  Side::Ask));
    EXPECT_EQ(book.spread(), 50u);
}

TEST(OrderBook, MultipleOrdersSamePriceAggregatesQuantity) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    book.insert(make_entry(2, 10000, 200, Side::Bid));
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 300u);
    EXPECT_EQ(book.order_ids_at(10000, Side::Bid).size(), 2u);
}

TEST(OrderBook, DuplicateOrderIdRejected) {
    OrderBook book(1);
    ASSERT_EQ(book.insert(make_entry(1, 10000, 100, Side::Bid)), OrderBookResult::Ok);
    EXPECT_EQ(book.insert(make_entry(1, 10000, 50,  Side::Bid)), OrderBookResult::OrderAlreadyExists);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 100u);  // unchanged
}

// ---------------------------------------------------------------------------
// Cancel correctness
// ---------------------------------------------------------------------------

TEST(OrderBook, CancelRemovesOrder) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    ASSERT_EQ(book.cancel(1), OrderBookResult::Ok);
    EXPECT_FALSE(book.has_bid());
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 0u);
}

TEST(OrderBook, CancelUpdatesBestBid) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    book.insert(make_entry(2, 9900,  100, Side::Bid));
    ASSERT_EQ(book.cancel(1), OrderBookResult::Ok);
    EXPECT_EQ(book.best_bid(), 9900u);
}

TEST(OrderBook, CancelUpdatesBestAsk) {
    OrderBook book(1);
    book.insert(make_entry(1, 10050, 50, Side::Ask));
    book.insert(make_entry(2, 10100, 50, Side::Ask));
    ASSERT_EQ(book.cancel(1), OrderBookResult::Ok);
    EXPECT_EQ(book.best_ask(), 10100u);
}

TEST(OrderBook, CancelNonExistentOrderReturnsNotFound) {
    OrderBook book(1);
    EXPECT_EQ(book.cancel(999), OrderBookResult::OrderNotFound);
}

TEST(OrderBook, CancelOneOfManyAtSamePrice) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    book.insert(make_entry(2, 10000, 200, Side::Bid));
    ASSERT_EQ(book.cancel(1), OrderBookResult::Ok);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 200u);
    EXPECT_EQ(book.best_bid(), 10000u);
}

TEST(OrderBook, CancelAllOrdersLeavesEmptyBook) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    book.insert(make_entry(2, 10050, 50,  Side::Ask));
    book.cancel(1);
    book.cancel(2);
    EXPECT_FALSE(book.has_bid());
    EXPECT_FALSE(book.has_ask());
}

// ---------------------------------------------------------------------------
// Modify correctness
// ---------------------------------------------------------------------------

TEST(OrderBook, ModifyChangesPrice) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    ASSERT_EQ(book.modify(1, 9900, 100), OrderBookResult::Ok);
    EXPECT_EQ(book.best_bid(), 9900u);
    EXPECT_EQ(book.quantity_at(9900,  Side::Bid), 100u);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 0u);
}

TEST(OrderBook, ModifyChangesQuantity) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    ASSERT_EQ(book.modify(1, 10000, 50), OrderBookResult::Ok);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 50u);
}

TEST(OrderBook, ModifyNonExistentOrderReturnsNotFound) {
    OrderBook book(1);
    EXPECT_EQ(book.modify(999, 10000, 100), OrderBookResult::OrderNotFound);
}

// ---------------------------------------------------------------------------
// Out-of-range price rejection (spec requirement)
// ---------------------------------------------------------------------------

TEST(OrderBook, InsertOutOfRangePriceRejected) {
    OrderBook book(1);
    EXPECT_EQ(book.insert(make_entry(1, 0, 100, Side::Bid)),
              OrderBookResult::PriceOutOfRange);
    EXPECT_EQ(book.insert(make_entry(2, OrderBook::MAX_PRICE + 1, 100, Side::Ask)),
              OrderBookResult::PriceOutOfRange);
    EXPECT_FALSE(book.has_bid());
    EXPECT_FALSE(book.has_ask());
}

TEST(OrderBook, ModifyToOutOfRangePriceRejected) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    EXPECT_EQ(book.modify(1, OrderBook::MAX_PRICE + 1, 100),
              OrderBookResult::PriceOutOfRange);
    // Book state must be unchanged
    EXPECT_EQ(book.best_bid(), 10000u);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 100u);
}

// ---------------------------------------------------------------------------
// Reduce (partial fill helper)
// ---------------------------------------------------------------------------

TEST(OrderBook, ReducePartialQuantity) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    ASSERT_EQ(book.reduce(1, 40), OrderBookResult::Ok);
    EXPECT_EQ(book.quantity_at(10000, Side::Bid), 60u);
    EXPECT_TRUE(book.has_bid());
}

TEST(OrderBook, ReduceToZeroCancelsOrder) {
    OrderBook book(1);
    book.insert(make_entry(1, 10000, 100, Side::Bid));
    ASSERT_EQ(book.reduce(1, 100), OrderBookResult::Ok);
    EXPECT_FALSE(book.has_bid());
}

// ---------------------------------------------------------------------------
// Depth snapshot
// ---------------------------------------------------------------------------

TEST(OrderBook, TopBidsDescendingOrder) {
    OrderBook book(1);
    book.insert(make_entry(1, 9900,  100, Side::Bid));
    book.insert(make_entry(2, 10000, 200, Side::Bid));
    book.insert(make_entry(3, 9950,  150, Side::Bid));

    auto bids = book.top_bids(3);
    ASSERT_EQ(bids.size(), 3u);
    EXPECT_EQ(bids[0].price, 10000u);
    EXPECT_EQ(bids[1].price, 9950u);
    EXPECT_EQ(bids[2].price, 9900u);
}

TEST(OrderBook, TopAsksAscendingOrder) {
    OrderBook book(1);
    book.insert(make_entry(1, 10100, 50, Side::Ask));
    book.insert(make_entry(2, 10050, 75, Side::Ask));
    book.insert(make_entry(3, 10200, 30, Side::Ask));

    auto asks = book.top_asks(3);
    ASSERT_EQ(asks.size(), 3u);
    EXPECT_EQ(asks[0].price, 10050u);
    EXPECT_EQ(asks[1].price, 10100u);
    EXPECT_EQ(asks[2].price, 10200u);
}

TEST(OrderBook, TopBidsLimitedToN) {
    OrderBook book(1);
    for (uint64_t i = 1; i <= 5; ++i)
        book.insert(make_entry(i, 10000 - i * 10, 100, Side::Bid));
    EXPECT_EQ(book.top_bids(2).size(), 2u);
}

// ---------------------------------------------------------------------------
// FIFO ordering within a price level
// ---------------------------------------------------------------------------

TEST(OrderBook, FIFOOrderingPreserved) {
    OrderBook book(1);
    book.insert(make_entry(10, 10000, 100, Side::Bid, 1000));
    book.insert(make_entry(20, 10000, 200, Side::Bid, 2000));
    book.insert(make_entry(30, 10000,  50, Side::Bid, 3000));

    const auto& ids = book.order_ids_at(10000, Side::Bid);
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 10u);
    EXPECT_EQ(ids[1], 20u);
    EXPECT_EQ(ids[2], 30u);
}

TEST(OrderBook, FindOrderReturnsCorrectEntry) {
    OrderBook book(1);
    book.insert(make_entry(42, 10000, 100, Side::Ask));
    const OrderEntry* e = book.find_order(42);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->price,    10000u);
    EXPECT_EQ(e->quantity, 100u);
    EXPECT_EQ(e->side,     Side::Ask);
}

TEST(OrderBook, FindOrderReturnsNullForUnknown) {
    OrderBook book(1);
    EXPECT_EQ(book.find_order(999), nullptr);
}
