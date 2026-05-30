#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include "engine/events/Event.hpp"

namespace trading {

enum class OrderBookResult : uint8_t {
    Ok,
    PriceOutOfRange,
    OrderNotFound,
    OrderAlreadyExists,
};

struct OrderEntry {
    uint64_t order_id;
    uint64_t price;
    uint64_t quantity;
    uint64_t timestamp;
    Side     side;
};

// One price level: total resting quantity + FIFO list of order IDs.
struct PriceLevel {
    uint64_t             total_quantity{0};
    std::vector<uint64_t> order_ids;     // insertion order = time priority
};

struct DepthEntry {
    uint64_t price;
    uint64_t quantity;
};

// Array-based order book with a bounded integer price range.
// Price range [MIN_PRICE, MAX_PRICE] is checked at insert time;
// out-of-range orders are rejected and never modify book state.
class OrderBook {
public:
    static constexpr uint64_t MIN_PRICE  = 1;
    static constexpr uint64_t MAX_PRICE  = 200'000;
    static constexpr uint64_t NUM_LEVELS = MAX_PRICE - MIN_PRICE + 1;

    explicit OrderBook(uint32_t symbol_id);

    // Insert a new order. Rejects if price is out of range or order_id already exists.
    OrderBookResult insert(const OrderEntry& order);

    // Cancel an existing order by ID.
    OrderBookResult cancel(uint64_t order_id);

    // Modify price/quantity of an existing order (cancel + re-insert, loses time priority on price change).
    OrderBookResult modify(uint64_t order_id, uint64_t new_price, uint64_t new_quantity);

    // Remove a quantity from an order (used by matching engine on partial fill).
    // Cancels the order when quantity reaches zero.
    OrderBookResult reduce(uint64_t order_id, uint64_t qty_to_remove);

    // --- Accessors ---

    uint64_t best_bid() const noexcept { return best_bid_; }
    uint64_t best_ask() const noexcept { return best_ask_; }
    bool     has_bid()  const noexcept { return best_bid_ >= MIN_PRICE; }
    bool     has_ask()  const noexcept { return best_ask_ <= MAX_PRICE; }

    uint64_t spread() const noexcept {
        return (has_bid() && has_ask()) ? best_ask_ - best_bid_ : 0;
    }

    uint64_t quantity_at(uint64_t price, Side side) const noexcept;

    // Returns the order IDs at a price level in FIFO order (for matching engine).
    const std::vector<uint64_t>& order_ids_at(uint64_t price, Side side) const;

    const OrderEntry* find_order(uint64_t order_id) const noexcept;

    // Top-N depth snapshot (bids descending, asks ascending).
    std::vector<DepthEntry> top_bids(std::size_t n) const;
    std::vector<DepthEntry> top_asks(std::size_t n) const;

    uint32_t symbol_id() const noexcept { return symbol_id_; }

private:
    bool        price_in_range(uint64_t price) const noexcept;
    std::size_t price_index(uint64_t price)    const noexcept;

    PriceLevel&       level(uint64_t price, Side side);
    const PriceLevel& level(uint64_t price, Side side) const;

    void remove_order_from_level(PriceLevel& lvl, uint64_t order_id, uint64_t quantity);
    void update_best_bid_from(uint64_t price);
    void update_best_ask_from(uint64_t price);

    uint32_t symbol_id_;

    std::vector<PriceLevel> bid_levels_;  // indexed by price - MIN_PRICE
    std::vector<PriceLevel> ask_levels_;

    std::unordered_map<uint64_t, OrderEntry> orders_;

    uint64_t best_bid_{0};              // 0 = no bids
    uint64_t best_ask_{MAX_PRICE + 1};  // MAX_PRICE+1 = no asks

    static const std::vector<uint64_t> empty_ids_;
};

} // namespace trading
