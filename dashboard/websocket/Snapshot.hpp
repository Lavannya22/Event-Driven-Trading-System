#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace trading::dashboard {

struct PriceQty {
    uint64_t price;
    uint64_t quantity;
};

struct OrderBookSnapshot {
    uint32_t             symbol_id{0};
    uint64_t             best_bid{0};
    uint64_t             best_ask{0};
    uint64_t             spread{0};
    std::vector<PriceQty> bids;  // top-10 descending
    std::vector<PriceQty> asks;  // top-10 ascending
};

struct TradeEvent {
    uint64_t timestamp{0};
    uint32_t symbol_id{0};
    uint64_t order_id{0};
    uint64_t price{0};
    uint64_t quantity{0};
    uint8_t  side{0};  // 0 = Bid, 1 = Ask
};

struct ReplayStats {
    uint64_t    events_replayed{0};
    uint64_t    last_timestamp{0};
    bool        done{false};
    std::string source_name;
};

struct Metrics {
    double   throughput_eps{0.0};   // events per second
    double   queue_occupancy{0.0};  // 0.0–1.0
    double   avg_latency_us{0.0};
    uint64_t signals{0};
    uint64_t noops{0};
};

struct PoolMetrics {
    std::string name;
    std::size_t capacity{0};
    std::size_t used{0};
    std::size_t available{0};
    std::size_t exhaustion_count{0};
};

struct StartupMetrics {
    bool        allocator_started{false};
    bool        mlockall_success{false};
    std::size_t event_pool_capacity{0};
    std::size_t order_pool_capacity{0};
    std::size_t execution_pool_capacity{0};
    std::size_t signal_pool_capacity{0};
    bool        validation_passed{false};
};

struct EngineSnapshot {
    static constexpr int       MAX_TRADES = 50;
    OrderBookSnapshot          orderbook;
    std::deque<TradeEvent>     recent_trades;
    ReplayStats                replay;
    Metrics                    metrics;
    std::vector<PoolMetrics>   pools;      // live pool utilisation
    StartupMetrics             startup;    // one-time startup report
    uint64_t                   snapshot_ts{0};  // wall-clock ms
};

} // namespace trading::dashboard
