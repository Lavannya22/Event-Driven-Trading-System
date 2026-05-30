#include <gtest/gtest.h>
#include <cstring>
#include "engine/events/Event.hpp"

using namespace trading;

TEST(EventModel, SizeIs40Bytes) {
    EXPECT_EQ(sizeof(Event), 40u);
}

TEST(EventModel, IsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<Event>);
}

TEST(EventModel, MakeNewOrder) {
    auto e = make_new_order(1'000'000, 1, 42, 10050, 100, Side::Bid);
    EXPECT_EQ(e.timestamp, 1'000'000u);
    EXPECT_EQ(e.symbol_id, 1u);
    EXPECT_EQ(e.order_id,  42u);
    EXPECT_EQ(e.price,     10050u);
    EXPECT_EQ(e.quantity,  100u);
    EXPECT_EQ(event_type(e), EventType::NewOrder);
    EXPECT_EQ(event_side(e), Side::Bid);
}

TEST(EventModel, NewOrderAskSide) {
    auto e = make_new_order(1'000'000, 1, 43, 10060, 50, Side::Ask);
    EXPECT_EQ(event_type(e), EventType::NewOrder);
    EXPECT_EQ(event_side(e), Side::Ask);
}

TEST(EventModel, MakeCancelOrder) {
    auto e = make_cancel_order(2'000'000, 1, 42);
    EXPECT_EQ(e.timestamp, 2'000'000u);
    EXPECT_EQ(e.order_id,  42u);
    EXPECT_EQ(e.price,     0u);
    EXPECT_EQ(e.quantity,  0u);
    EXPECT_EQ(event_type(e), EventType::CancelOrder);
}

TEST(EventModel, MakeModifyOrder) {
    auto e = make_modify_order(3'000'000, 1, 42, 10060, 50, Side::Ask);
    EXPECT_EQ(e.price,    10060u);
    EXPECT_EQ(e.quantity, 50u);
    EXPECT_EQ(event_type(e), EventType::ModifyOrder);
    EXPECT_EQ(event_side(e), Side::Ask);
}

TEST(EventModel, MakeMarketUpdate) {
    auto e = make_market_update(4'000'000, 1, 10070, 200);
    EXPECT_EQ(e.order_id, 0u);
    EXPECT_EQ(event_type(e), EventType::MarketUpdate);
}

TEST(EventModel, MakeTradeExecution) {
    auto e = make_trade_execution(5'000'000, 1, 99, 10050, 75);
    EXPECT_EQ(e.order_id,  99u);
    EXPECT_EQ(e.price,     10050u);
    EXPECT_EQ(e.quantity,  75u);
    EXPECT_EQ(event_type(e), EventType::TradeExecution);
}

TEST(EventModel, SideEncodingDoesNotCorruptType) {
    auto bid = make_new_order(0, 1, 1, 100, 10, Side::Bid);
    auto ask = make_new_order(0, 1, 2, 100, 10, Side::Ask);
    EXPECT_EQ(event_type(bid), EventType::NewOrder);
    EXPECT_EQ(event_type(ask), EventType::NewOrder);
    EXPECT_NE(bid.type, ask.type);
}

TEST(EventModel, MemcpySafe) {
    auto src = make_new_order(1'000'000, 2, 7, 9999, 10, Side::Ask);
    Event dst{};
    std::memcpy(&dst, &src, sizeof(Event));
    EXPECT_EQ(dst.timestamp, src.timestamp);
    EXPECT_EQ(dst.symbol_id, src.symbol_id);
    EXPECT_EQ(dst.order_id,  src.order_id);
    EXPECT_EQ(dst.price,     src.price);
    EXPECT_EQ(dst.quantity,  src.quantity);
    EXPECT_EQ(dst.type,      src.type);
    EXPECT_EQ(event_side(dst), Side::Ask);
}

TEST(EventModel, ZeroInitByDefault) {
    Event e{};
    EXPECT_EQ(e.timestamp, 0u);
    EXPECT_EQ(e.symbol_id, 0u);
    EXPECT_EQ(e.type,      0u);
    EXPECT_EQ(e.price,     0u);
    EXPECT_EQ(e.quantity,  0u);
    EXPECT_EQ(e.order_id,  0u);
}
