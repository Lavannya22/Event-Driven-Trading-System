#pragma once

#include <cstdint>
#include <type_traits>

// Cache-line-aligned POD structs for pool allocation.
// All types are exactly 64B (one cache line) except StrategySignal (32B).
// None carry heap ownership — they are pool-managed exclusively.

namespace trading {

// ── Order ─────────────────────────────────────────────────────────────────────
// Represents a resting limit order inside the order book.

struct alignas(64) Order {
    uint64_t order_id;      // unique order identifier
    uint64_t price;         // limit price
    uint64_t quantity;      // original quantity
    uint64_t remaining;     // unfilled quantity
    uint64_t timestamp;     // insertion timestamp (ns)
    uint64_t sequence;      // order book insertion sequence
    uint32_t symbol_id;
    uint32_t type;          // EventType | Side (same encoding as Event.type)
    uint8_t  _reserved[8];
};

static_assert(sizeof(Order) == 64,          "Order must be exactly 64B");
static_assert(alignof(Order) == 64,         "Order must be cache-line aligned");
static_assert(std::is_trivially_copyable_v<Order>, "Order must be trivially copyable");

// ── ExecutionReport ───────────────────────────────────────────────────────────
// Produced by the matching engine for each fill.

struct alignas(64) ExecutionReport {
    uint64_t aggressor_order_id;           // offset  0
    uint64_t resting_order_id;             // offset  8
    uint64_t price;                        // offset 16 — execution price
    uint64_t quantity;                     // offset 24
    uint64_t timestamp;                    // offset 32
    uint64_t sequence;                     // offset 40 — global execution sequence
    uint32_t symbol_id;                    // offset 48
    uint8_t  aggressor_side;              // offset 52 — 0=Bid, 1=Ask
    uint8_t  _pad[3];                      // offset 53
    uint64_t _reserved;                    // offset 56
};                                         // total: 64B

static_assert(sizeof(ExecutionReport) == 64,          "ExecutionReport must be exactly 64B");
static_assert(alignof(ExecutionReport) == 64,         "ExecutionReport must be cache-line aligned");
static_assert(std::is_trivially_copyable_v<ExecutionReport>, "ExecutionReport must be trivially copyable");

// ── SignalRecord ──────────────────────────────────────────────────────────────
// Pool-managed record carrying a strategy decision and source event metadata.
// 32B — two fit in one cache line, suitable for high-throughput signal queues.
// Named SignalRecord to avoid collision with the enum class StrategySignal
// defined in engine/strategy/Strategy.hpp.

struct alignas(32) SignalRecord {
    uint8_t  signal;        // 0 = Noop, 1 = Signal
    uint8_t  _pad[7];
    uint64_t event_index;   // index into the event pool
    uint64_t timestamp;     // when the signal was produced
    uint64_t _reserved;
};

static_assert(sizeof(SignalRecord) == 32,          "SignalRecord must be exactly 32B");
static_assert(alignof(SignalRecord) == 32,         "SignalRecord must be 32B aligned");
static_assert(std::is_trivially_copyable_v<SignalRecord>, "SignalRecord must be trivially copyable");

} // namespace trading
