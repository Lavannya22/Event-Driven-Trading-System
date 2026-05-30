#pragma once

#include "engine/events/Event.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace trading {

// Per-symbol configuration supplied at startup.
struct SymbolConfig {
    uint32_t symbol_id;
    uint64_t base_price;    // centre of the bounded price range
    uint32_t tick_shift;    // reserved for Phase 3 shift-based indexing
    uint32_t price_levels;  // total levels in the order book
};

// Bundles the book, strategy and matcher for a single instrument.
struct SymbolPipeline {
    std::unique_ptr<OrderBook>      book;
    std::unique_ptr<StrategyEngine> strategy;
    MatchingEngine                  matcher;

    SymbolPipeline(uint32_t symbol_id,
                   std::unique_ptr<Strategy> strat)
        : book(std::make_unique<OrderBook>(symbol_id))
        , strategy(std::make_unique<StrategyEngine>(std::move(strat)))
    {}
};

// Routes incoming events to the correct per-symbol pipeline.
// Events for one symbol never affect another symbol's state.
//
// Lookup is O(1) via a fixed-size table indexed by symbol_id.
// Maximum symbol_id supported: kMaxSymbols - 1.
class SymbolRouter {
public:
    static constexpr uint32_t kMaxSymbols = 256;

    // Register a symbol. Must be called before start().
    // Takes ownership of the strategy; creates the pipeline internally.
    bool register_symbol(const SymbolConfig& cfg,
                         std::unique_ptr<Strategy> strategy);

    // Process one event. Routes to the correct pipeline and returns the
    // MatchOutput (fills + possible STP cancel). Returns empty output if the
    // symbol is not registered or the strategy gates the event.
    MatchOutput process(const Event& event);

    // Returns the OrderBook for symbol_id, or nullptr if not registered.
    OrderBook* book(uint32_t symbol_id) noexcept;

    // Returns the StrategyEngine for symbol_id, or nullptr if not registered.
    StrategyEngine* strategy(uint32_t symbol_id) noexcept;

    std::size_t registered_count() const noexcept { return count_; }

private:
    // Slot stores optional pipeline. Using unique_ptr so unregistered slots
    // are nullptr without a separate bool array.
    std::unique_ptr<SymbolPipeline> slots_[kMaxSymbols];
    std::size_t count_{0};
};

} // namespace trading
