#include "engine/matching/MatchingEngine.hpp"
#include <algorithm>
#include <array>

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
        if (agg_side == Side::Bid) {
            if (!book.has_ask() || book.best_ask() > agg_price) break;
        } else {
            if (!book.has_bid() || book.best_bid() < agg_price) break;
        }

        const uint64_t match_price = (agg_side == Side::Bid)
                                        ? book.best_ask()
                                        : book.best_bid();

        // Snapshot order IDs to a stack array — no heap allocation.
        std::array<uint64_t, OrderBook::MAX_ORDERS_PER_LEVEL> snap{};
        std::size_t snap_count = 0;
        for (uint64_t id : book.order_ids_at(match_price, opp_side))
            if (snap_count < snap.size()) snap[snap_count++] = id;

        bool stp_hit = false;
        for (std::size_t i = 0; i < snap_count; ++i) {
            if (remaining_qty == 0) break;

            // Phase 3: prefetch next order entry 1 iteration ahead.
            if (i + 1 < snap_count) {
                const OrderEntry* next = book.find_order(snap[i + 1]);
                if (next) __builtin_prefetch(next, 0, 1);
            }

            const uint64_t resting_id = snap[i];
            const OrderEntry* resting = book.find_order(resting_id);
            if (!resting) continue;

            if (is_self_trade(agg_id, resting_id)) {
                out.push(make_cancel_order(ts, sym, agg_id));
                out.result = MatchResult::STPTriggered;
                stp_hit = true;
                break;
            }

            const uint64_t fill_qty = std::min(remaining_qty, resting->quantity);
            out.push(make_trade_execution(ts, sym, agg_id, match_price, fill_qty, agg_side));
            book.reduce(resting_id, fill_qty);
            remaining_qty -= fill_qty;
        }

        if (stp_hit) return out;
    }

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
