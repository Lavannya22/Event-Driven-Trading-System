#pragma once

#include <cstdint>
#include <vector>
#include "engine/events/Event.hpp"
#include "engine/orderbook/OrderBook.hpp"

namespace trading {

// Participant ID is encoded in the upper 16 bits of order_id,
// keeping the Event struct at 40 bytes.
//   order_id layout: [63:48] participant_id | [47:0] sequence
// Participant 0 is reserved as "anonymous" — STP is not checked for it.
inline uint16_t participant_of(uint64_t order_id) noexcept {
    return static_cast<uint16_t>(order_id >> 48);
}

inline uint64_t make_order_id(uint16_t participant_id, uint64_t sequence) noexcept {
    return (static_cast<uint64_t>(participant_id) << 48)
         | (sequence & 0x0000FFFFFFFFFFFFull);
}

enum class MatchResult : uint8_t {
    Ok,
    STPTriggered,   // self-trade detected, aggressor cancelled
    OrderRejected,  // order book rejected the insert (out of range, duplicate id)
};

struct MatchOutput {
    MatchResult        result{MatchResult::Ok};
    // TradeExecution events + possibly a CancelOrder event on STP.
    // Downstream pipeline reads these in order.
    std::vector<Event> events;
};

class MatchingEngine {
public:
    // Match an incoming NewOrder event against the book using price-time priority.
    // Unfilled remainder is inserted as a resting order.
    // On STP: emits a CancelOrder for the aggressor and returns STPTriggered.
    MatchOutput process_new_order(OrderBook& book, const Event& aggressor);

    // Forward a CancelOrder event directly to the book.
    OrderBookResult process_cancel(OrderBook& book, const Event& event);

    // Forward a ModifyOrder event directly to the book.
    OrderBookResult process_modify(OrderBook& book, const Event& event);

private:
    // Two orders self-trade if they share the same non-zero participant ID.
    static bool is_self_trade(uint64_t aggressor_id, uint64_t resting_id) noexcept;
};

} // namespace trading
