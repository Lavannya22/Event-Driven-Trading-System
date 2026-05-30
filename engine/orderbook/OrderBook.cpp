#include "engine/orderbook/OrderBook.hpp"
#include <algorithm>

namespace trading {

OrderBook::OrderBook(uint32_t symbol_id, std::size_t max_orders, uint32_t tick_shift)
    : symbol_id_(symbol_id)
    , tick_shift_(tick_shift)
    , bid_levels_(NUM_LEVELS >> tick_shift)
    , ask_levels_(NUM_LEVELS >> tick_shift)
    , orders_(max_orders * 2)   // 2× capacity for 50% load factor
{}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

OrderBookResult OrderBook::insert(const OrderEntry& order) {
    if (!price_in_range(order.price))
        return OrderBookResult::PriceOutOfRange;

    if (orders_.find(order.order_id))
        return OrderBookResult::OrderAlreadyExists;

    if (!orders_.insert(order.order_id, order))
        return OrderBookResult::OrderAlreadyExists;  // map full

    PriceLevel& lvl = level(order.price, order.side);
    lvl.total_quantity += order.quantity;
    lvl.push(order.order_id);

    if (order.side == Side::Bid && order.price > best_bid_)
        best_bid_ = order.price;
    else if (order.side == Side::Ask && order.price < best_ask_)
        best_ask_ = order.price;

    return OrderBookResult::Ok;
}

OrderBookResult OrderBook::cancel(uint64_t order_id) {
    OrderEntry* o = orders_.find(order_id);
    if (!o) return OrderBookResult::OrderNotFound;

    PriceLevel& lvl    = level(o->price, o->side);
    const uint64_t price = o->price;
    const Side     side  = o->side;
    const uint64_t qty   = o->quantity;

    lvl.total_quantity -= qty;
    lvl.remove(order_id);
    orders_.erase(order_id);

    if (lvl.total_quantity == 0) {
        if (side == Side::Bid && price == best_bid_)
            update_best_bid_from(price);
        else if (side == Side::Ask && price == best_ask_)
            update_best_ask_from(price);
    }

    return OrderBookResult::Ok;
}

OrderBookResult OrderBook::modify(uint64_t order_id,
                                   uint64_t new_price, uint64_t new_quantity) {
    if (!price_in_range(new_price))
        return OrderBookResult::PriceOutOfRange;

    OrderEntry* o = orders_.find(order_id);
    if (!o) return OrderBookResult::OrderNotFound;

    const OrderEntry old = *o;

    PriceLevel& old_lvl = level(old.price, old.side);
    old_lvl.total_quantity -= old.quantity;
    old_lvl.remove(order_id);
    orders_.erase(order_id);

    if (old_lvl.total_quantity == 0) {
        if (old.side == Side::Bid && old.price == best_bid_)
            update_best_bid_from(old.price);
        else if (old.side == Side::Ask && old.price == best_ask_)
            update_best_ask_from(old.price);
    }

    OrderEntry updated   = old;
    updated.price        = new_price;
    updated.quantity     = new_quantity;
    return insert(updated);
}

OrderBookResult OrderBook::reduce(uint64_t order_id, uint64_t qty_to_remove) {
    OrderEntry* o = orders_.find(order_id);
    if (!o) return OrderBookResult::OrderNotFound;

    if (qty_to_remove >= o->quantity)
        return cancel(order_id);

    level(o->price, o->side).total_quantity -= qty_to_remove;
    o->quantity -= qty_to_remove;
    return OrderBookResult::Ok;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

uint64_t OrderBook::quantity_at(uint64_t price, Side side) const noexcept {
    if (!price_in_range(price)) return 0;
    return level(price, side).total_quantity;
}

std::span<const uint64_t> OrderBook::order_ids_at(uint64_t price, Side side) const noexcept {
    if (!price_in_range(price)) return {};
    return level(price, side).ids();
}

const OrderEntry* OrderBook::find_order(uint64_t order_id) const noexcept {
    return orders_.find(order_id);
}

std::vector<DepthEntry> OrderBook::top_bids(std::size_t n) const {
    std::vector<DepthEntry> result;
    result.reserve(n);
    if (!has_bid()) return result;

    for (uint64_t p = best_bid_; p >= MIN_PRICE && result.size() < n; --p) {
        if (bid_levels_[price_index(p)].total_quantity > 0)
            result.push_back({p, bid_levels_[price_index(p)].total_quantity});
        if (p == MIN_PRICE) break;
    }
    return result;
}

std::vector<DepthEntry> OrderBook::top_asks(std::size_t n) const {
    std::vector<DepthEntry> result;
    result.reserve(n);
    if (!has_ask()) return result;

    for (uint64_t p = best_ask_; p <= MAX_PRICE && result.size() < n; ++p) {
        if (ask_levels_[price_index(p)].total_quantity > 0)
            result.push_back({p, ask_levels_[price_index(p)].total_quantity});
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool OrderBook::price_in_range(uint64_t price) const noexcept {
    return price >= MIN_PRICE && price <= MAX_PRICE;
}

// Phase 3: bit-shift replaces division — no div/idiv instruction generated.
// tick_shift=0 → index = price - MIN_PRICE  (tick size = 1, every price valid)
// tick_shift=N → index = (price - MIN_PRICE) >> N  (tick size = 2^N)
std::size_t OrderBook::price_index(uint64_t price) const noexcept {
    return static_cast<std::size_t>((price - MIN_PRICE) >> tick_shift_);
}

PriceLevel& OrderBook::level(uint64_t price, Side side) noexcept {
    return side == Side::Bid ? bid_levels_[price_index(price)]
                             : ask_levels_[price_index(price)];
}

const PriceLevel& OrderBook::level(uint64_t price, Side side) const noexcept {
    return side == Side::Bid ? bid_levels_[price_index(price)]
                             : ask_levels_[price_index(price)];
}

void OrderBook::update_best_bid_from(uint64_t price) noexcept {
    for (uint64_t p = price; p >= MIN_PRICE; --p) {
        if (bid_levels_[price_index(p)].total_quantity > 0) {
            best_bid_ = p;
            return;
        }
        if (p == MIN_PRICE) break;
    }
    best_bid_ = 0;
}

void OrderBook::update_best_ask_from(uint64_t price) noexcept {
    for (uint64_t p = price + 1; p <= MAX_PRICE; ++p) {
        if (ask_levels_[price_index(p)].total_quantity > 0) {
            best_ask_ = p;
            return;
        }
    }
    best_ask_ = MAX_PRICE + 1;
}

} // namespace trading
