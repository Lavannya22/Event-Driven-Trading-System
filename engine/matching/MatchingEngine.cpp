#include "engine/matching/MatchingEngine.hpp"
#include <algorithm>

namespace trading {

MatchOutput MatchingEngine::process_new_order(OrderBook& book, const Event& aggressor) {
    MatchOutput out;

    const Side     agg_side  = event_side(aggressor);
    const Side     opp_side  = (agg_side == Side::Bid) ? Side::Ask : Side::Bid;
    const uint64_t agg_price = aggressor.price;
    const uint64_t agg_id    = aggressor.order_id;
    const uint64_t ts        = aggressor.timestamp;
    const uint32_t sym       = aggressor.symbol_id;

    uint64_t remaining_qty = aggressor.quantity;

    while (remaining_qty > 0) {
        // Check whether the best opposite-side price crosses the aggressor's limit.
        if (agg_side == Side::Bid) {
            if (!book.has_ask() || book.best_ask() > agg_price) break;
        } else {
            if (!book.has_bid() || book.best_bid() < agg_price) break;
        }

        const uint64_t match_price = (agg_side == Side::Bid)
                                        ? book.best_ask()
                                        : book.best_bid();

        // Snapshot FIFO order list to avoid invalidation while we reduce orders.
        const std::vector<uint64_t> level_ids =
            book.order_ids_at(match_price, opp_side);

        bool stp_hit = false;
        for (uint64_t resting_id : level_ids) {
            if (remaining_qty == 0) break;

            const OrderEntry* resting = book.find_order(resting_id);
            if (!resting) continue;  // fully consumed earlier in this loop

            // Self-trade prevention — cancel aggressor for its remaining quantity.
            if (is_self_trade(agg_id, resting_id)) {
                out.events.push_back(make_cancel_order(ts, sym, agg_id));
                out.result = MatchResult::STPTriggered;
                stp_hit = true;
                break;
            }

            const uint64_t fill_qty = std::min(remaining_qty, resting->quantity);
            out.events.push_back(
                make_trade_execution(ts, sym, agg_id, match_price, fill_qty));
            book.reduce(resting_id, fill_qty);
            remaining_qty -= fill_qty;
        }

        if (stp_hit) return out;
    }

    // Insert any unfilled remainder as a new resting order.
    if (remaining_qty > 0) {
        const OrderEntry entry{agg_id, agg_price, remaining_qty, ts, agg_side};
        if (book.insert(entry) != OrderBookResult::Ok)
            out.result = MatchResult::OrderRejected;
    }

    return out;
}

OrderBookResult MatchingEngine::process_cancel(OrderBook& book, const Event& event) {
    return book.cancel(event.order_id);
}

OrderBookResult MatchingEngine::process_modify(OrderBook& book, const Event& event) {
    return book.modify(event.order_id, event.price, event.quantity);
}

bool MatchingEngine::is_self_trade(uint64_t aggressor_id,
                                    uint64_t resting_id) noexcept {
    const uint16_t ap = participant_of(aggressor_id);
    return ap != 0 && ap == participant_of(resting_id);
}

} // namespace trading
