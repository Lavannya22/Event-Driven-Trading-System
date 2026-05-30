#include "engine/orderbook/OrderBook.hpp"
#include <algorithm>
#include <stdexcept>

namespace trading {

const std::vector<uint64_t> OrderBook::empty_ids_{};

OrderBook::OrderBook(uint32_t symbol_id)
    : symbol_id_(symbol_id)
    , bid_levels_(NUM_LEVELS)
    , ask_levels_(NUM_LEVELS)
{
    orders_.reserve(4096);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

OrderBookResult OrderBook::insert(const OrderEntry& order) {
    if (!price_in_range(order.price))
        return OrderBookResult::PriceOutOfRange;

    if (orders_.count(order.order_id))
        return OrderBookResult::OrderAlreadyExists;

    orders_.emplace(order.order_id, order);

    PriceLevel& lvl = level(order.price, order.side);
    lvl.total_quantity += order.quantity;
    lvl.order_ids.push_back(order.order_id);

    if (order.side == Side::Bid && order.price > best_bid_)
        best_bid_ = order.price;
    else if (order.side == Side::Ask && order.price < best_ask_)
        best_ask_ = order.price;

    return OrderBookResult::Ok;
}

OrderBookResult OrderBook::cancel(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return OrderBookResult::OrderNotFound;

    const OrderEntry& o = it->second;
    PriceLevel& lvl = level(o.price, o.side);

    remove_order_from_level(lvl, order_id, o.quantity);

    const uint64_t price = o.price;
    const Side     side  = o.side;
    orders_.erase(it);

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

    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return OrderBookResult::OrderNotFound;

    // Capture before erasing
    const OrderEntry old = it->second;

    // Remove from current level
    PriceLevel& old_lvl = level(old.price, old.side);
    remove_order_from_level(old_lvl, order_id, old.quantity);
    orders_.erase(it);

    if (old_lvl.total_quantity == 0) {
        if (old.side == Side::Bid && old.price == best_bid_)
            update_best_bid_from(old.price);
        else if (old.side == Side::Ask && old.price == best_ask_)
            update_best_ask_from(old.price);
    }

    // Re-insert at new price (loses time priority — acceptable for Phase 1 modify)
    OrderEntry updated = old;
    updated.price    = new_price;
    updated.quantity = new_quantity;
    return insert(updated);
}

OrderBookResult OrderBook::reduce(uint64_t order_id, uint64_t qty_to_remove) {
    auto it = orders_.find(order_id);
    if (it == orders_.end())
        return OrderBookResult::OrderNotFound;

    OrderEntry& o = it->second;

    if (qty_to_remove >= o.quantity)
        return cancel(order_id);

    PriceLevel& lvl = level(o.price, o.side);
    lvl.total_quantity -= qty_to_remove;
    o.quantity         -= qty_to_remove;

    return OrderBookResult::Ok;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

uint64_t OrderBook::quantity_at(uint64_t price, Side side) const noexcept {
    if (!price_in_range(price)) return 0;
    return level(price, side).total_quantity;
}

const std::vector<uint64_t>& OrderBook::order_ids_at(uint64_t price, Side side) const {
    if (!price_in_range(price)) return empty_ids_;
    return level(price, side).order_ids;
}

const OrderEntry* OrderBook::find_order(uint64_t order_id) const noexcept {
    auto it = orders_.find(order_id);
    return it != orders_.end() ? &it->second : nullptr;
}

std::vector<DepthEntry> OrderBook::top_bids(std::size_t n) const {
    std::vector<DepthEntry> result;
    result.reserve(n);
    if (!has_bid()) return result;

    for (uint64_t p = best_bid_; p >= MIN_PRICE && result.size() < n; --p) {
        const PriceLevel& lvl = bid_levels_[price_index(p)];
        if (lvl.total_quantity > 0)
            result.push_back({p, lvl.total_quantity});
        if (p == MIN_PRICE) break;
    }
    return result;
}

std::vector<DepthEntry> OrderBook::top_asks(std::size_t n) const {
    std::vector<DepthEntry> result;
    result.reserve(n);
    if (!has_ask()) return result;

    for (uint64_t p = best_ask_; p <= MAX_PRICE && result.size() < n; ++p)  {
        const PriceLevel& lvl = ask_levels_[price_index(p)];
        if (lvl.total_quantity > 0)
            result.push_back({p, lvl.total_quantity});
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool OrderBook::price_in_range(uint64_t price) const noexcept {
    return price >= MIN_PRICE && price <= MAX_PRICE;
}

std::size_t OrderBook::price_index(uint64_t price) const noexcept {
    return static_cast<std::size_t>(price - MIN_PRICE);
}

PriceLevel& OrderBook::level(uint64_t price, Side side) {
    return side == Side::Bid ? bid_levels_[price_index(price)]
                             : ask_levels_[price_index(price)];
}

const PriceLevel& OrderBook::level(uint64_t price, Side side) const {
    return side == Side::Bid ? bid_levels_[price_index(price)]
                             : ask_levels_[price_index(price)];
}

void OrderBook::remove_order_from_level(PriceLevel& lvl,
                                         uint64_t order_id, uint64_t quantity) {
    lvl.total_quantity -= quantity;
    auto& ids = lvl.order_ids;
    ids.erase(std::remove(ids.begin(), ids.end(), order_id), ids.end());
}

void OrderBook::update_best_bid_from(uint64_t price) {
    for (uint64_t p = price; p >= MIN_PRICE; --p) {
        if (bid_levels_[price_index(p)].total_quantity > 0) {
            best_bid_ = p;
            return;
        }
        if (p == MIN_PRICE) break;
    }
    best_bid_ = 0;
}

void OrderBook::update_best_ask_from(uint64_t price) {
    for (uint64_t p = price + 1; p <= MAX_PRICE; ++p) {
        if (ask_levels_[price_index(p)].total_quantity > 0) {
            best_ask_ = p;
            return;
        }
    }
    best_ask_ = MAX_PRICE + 1;
}

} // namespace trading
