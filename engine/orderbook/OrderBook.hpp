#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "engine/events/Event.hpp"
#include "engine/orderbook/FlatOrderMap.hpp"

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
// Fixed-size array eliminates per-level heap allocation.
struct PriceLevel {
    static constexpr std::size_t MAX_ORDERS = 16;

    uint64_t total_quantity{0};
    std::array<uint64_t, MAX_ORDERS> order_ids{};
    uint8_t  count{0};

    std::span<const uint64_t> ids() const noexcept {
        return {order_ids.data(), count};
    }

    bool push(uint64_t id) noexcept {
        if (count >= MAX_ORDERS) return false;
        order_ids[count++] = id;
        return true;
    }

    void remove(uint64_t id) noexcept {
        for (uint8_t i = 0; i < count; ++i) {
            if (order_ids[i] == id) {
                // Shift remaining IDs left (preserve FIFO order).
                for (uint8_t j = i; j + 1 < count; ++j)
                    order_ids[j] = order_ids[j + 1];
                --count;
                return;
            }
        }
    }
};

struct DepthEntry {
    uint64_t price;
    uint64_t quantity;
};

// Array-based order book with a bounded integer price range.
// Phase 2: allocation-free hot path.
//   - PriceLevel uses a fixed inline array (no per-level vector)
//   - Order lookup uses FlatOrderMap (pre-allocated at construction)
class OrderBook {
public:
    static constexpr uint64_t MIN_PRICE          = 1;
    static constexpr uint64_t MAX_PRICE          = 200'000;
    static constexpr uint64_t NUM_LEVELS         = MAX_PRICE - MIN_PRICE + 1;
    static constexpr std::size_t MAX_ORDERS_PER_LEVEL = PriceLevel::MAX_ORDERS;
    static constexpr std::size_t DEFAULT_MAX_ORDERS   = 65536;

    explicit OrderBook(uint32_t symbol_id,
                       std::size_t max_orders = DEFAULT_MAX_ORDERS);

    OrderBookResult insert(const OrderEntry& order);
    OrderBookResult cancel(uint64_t order_id);
    OrderBookResult modify(uint64_t order_id, uint64_t new_price, uint64_t new_quantity);
    OrderBookResult reduce(uint64_t order_id, uint64_t qty_to_remove);

    uint64_t best_bid() const noexcept { return best_bid_; }
    uint64_t best_ask() const noexcept { return best_ask_; }
    bool     has_bid()  const noexcept { return best_bid_ >= MIN_PRICE; }
    bool     has_ask()  const noexcept { return best_ask_ <= MAX_PRICE; }

    uint64_t spread() const noexcept {
        return (has_bid() && has_ask()) ? best_ask_ - best_bid_ : 0;
    }

    uint64_t quantity_at(uint64_t price, Side side) const noexcept;

    // Returns a span of order IDs at a price level in FIFO order.
    std::span<const uint64_t> order_ids_at(uint64_t price, Side side) const noexcept;

    const OrderEntry* find_order(uint64_t order_id) const noexcept;

    std::vector<DepthEntry> top_bids(std::size_t n) const;
    std::vector<DepthEntry> top_asks(std::size_t n) const;

    uint32_t symbol_id() const noexcept { return symbol_id_; }

private:
    bool        price_in_range(uint64_t price) const noexcept;
    std::size_t price_index(uint64_t price)    const noexcept;

    PriceLevel&       level(uint64_t price, Side side)       noexcept;
    const PriceLevel& level(uint64_t price, Side side) const noexcept;

    void update_best_bid_from(uint64_t price) noexcept;
    void update_best_ask_from(uint64_t price) noexcept;

    uint32_t symbol_id_;

    std::vector<PriceLevel>           bid_levels_;
    std::vector<PriceLevel>           ask_levels_;
    FlatOrderMap<OrderEntry>          orders_;

    uint64_t best_bid_{0};
    uint64_t best_ask_{MAX_PRICE + 1};
};

} // namespace trading
