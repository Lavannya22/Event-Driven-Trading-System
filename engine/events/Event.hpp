#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace trading {

enum class EventType : uint32_t {
    NewOrder       = 1,
    CancelOrder    = 2,
    ModifyOrder    = 3,
    MarketUpdate   = 4,
    TradeExecution = 5,
};

enum class Side : uint8_t {
    Bid = 0,
    Ask = 1,
};

// Fixed-size cache-line-aligned POD event. All engine communication flows
// through this struct. Must remain memcpy-safe.
//
// type field bit layout:
//   [15: 0]  EventType value (1–5)
//   [16]     Side: 0 = Bid, 1 = Ask
//   [31:17]  reserved (0)
//
// Phase 2: expanded from 40B to 64B (one full cache line) to eliminate
// false sharing when events are passed through the SPSC queue.
struct alignas(64) Event {
    uint64_t timestamp;   // nanoseconds since epoch
    uint32_t symbol_id;
    uint32_t type;        // EventType in low 16 bits, Side in bit 16
    uint64_t price;       // integer price (e.g. cents or ticks)
    uint64_t quantity;
    uint64_t order_id;
    uint64_t sequence;    // global sequence number (0 = unset)
    uint64_t reserved;    // reserved for Phase 3 expansion
};

static_assert(sizeof(Event) == 64,                     "Event must be exactly one cache line (64B)");
static_assert(alignof(Event) == 64,                    "Event must be cache-line aligned");
static_assert(std::is_trivially_copyable_v<Event>,     "Event must be trivially copyable");
static_assert(std::is_standard_layout_v<Event>,        "Event must have standard layout");

inline EventType event_type(const Event& e) noexcept {
    return static_cast<EventType>(e.type & 0xFFFFu);
}

inline Side event_side(const Event& e) noexcept {
    return static_cast<Side>((e.type >> 16) & 0x1u);
}

inline uint32_t encode_type(EventType t, Side s = Side::Bid) noexcept {
    return static_cast<uint32_t>(t) | (static_cast<uint32_t>(s) << 16);
}

inline Event make_new_order(uint64_t timestamp, uint32_t symbol_id,
                             uint64_t order_id, uint64_t price, uint64_t quantity,
                             Side side) noexcept {
    Event e{};
    e.timestamp = timestamp;
    e.symbol_id = symbol_id;
    e.type      = encode_type(EventType::NewOrder, side);
    e.price     = price;
    e.quantity  = quantity;
    e.order_id  = order_id;
    return e;
}

inline Event make_cancel_order(uint64_t timestamp, uint32_t symbol_id,
                                uint64_t order_id) noexcept {
    Event e{};
    e.timestamp = timestamp;
    e.symbol_id = symbol_id;
    e.type      = encode_type(EventType::CancelOrder);
    e.order_id  = order_id;
    return e;
}

inline Event make_modify_order(uint64_t timestamp, uint32_t symbol_id,
                                uint64_t order_id, uint64_t new_price, uint64_t new_quantity,
                                Side side) noexcept {
    Event e{};
    e.timestamp = timestamp;
    e.symbol_id = symbol_id;
    e.type      = encode_type(EventType::ModifyOrder, side);
    e.price     = new_price;
    e.quantity  = new_quantity;
    e.order_id  = order_id;
    return e;
}

inline Event make_market_update(uint64_t timestamp, uint32_t symbol_id,
                                 uint64_t price, uint64_t quantity) noexcept {
    Event e{};
    e.timestamp = timestamp;
    e.symbol_id = symbol_id;
    e.type      = encode_type(EventType::MarketUpdate);
    e.price     = price;
    e.quantity  = quantity;
    return e;
}

inline Event make_trade_execution(uint64_t timestamp, uint32_t symbol_id,
                                   uint64_t order_id, uint64_t price, uint64_t quantity) noexcept {
    Event e{};
    e.timestamp = timestamp;
    e.symbol_id = symbol_id;
    e.type      = encode_type(EventType::TradeExecution);
    e.price     = price;
    e.quantity  = quantity;
    e.order_id  = order_id;
    return e;
}

} // namespace trading
