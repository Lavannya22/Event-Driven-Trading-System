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
    double   throughput_eps{0.0};     // events per second
    double   queue_occupancy{0.0};    // 0.0–1.0
    double   avg_latency_us{0.0};     // EMA of hot-path latency

    // Live latency histogram percentiles (nanoseconds).
    // Measured from event arrival (before strategy) to matching complete.
    uint64_t p50_ns{0};
    uint64_t p99_ns{0};
    uint64_t p999_ns{0};
    uint64_t max_ns{0};

    // Tick-to-trade: time from any event arriving to a TradeExecution
    // being produced within the same processing cycle (nanoseconds).
    uint64_t tick_to_trade_p50_ns{0};
    uint64_t tick_to_trade_p99_ns{0};
    uint64_t tick_to_trade_max_ns{0};

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

// ── Phase 4 snapshot structs ──────────────────────────────────────────────────

struct RunResultSummary {
    uint32_t run_number{0};
    uint32_t window_size{0};
    double   threshold{0.0};
    uint64_t total_trades{0};
    double   realized_pnl{0.0};
    double   sharpe_ratio{0.0};
    double   win_rate{0.0};
    double   max_drawdown{0.0};
};

struct BacktestSnapshot {
    std::vector<RunResultSummary> results;
    bool     running{false};
    uint32_t current_run{0};
    uint32_t total_runs{0};
};

struct StabilitySnapshot {
    double      latency_drift_pct{0.0};
    double      throughput_drift_pct{0.0};
    std::size_t memory_current_mb{0};
    std::size_t memory_peak_mb{0};
    double      memory_growth_rate_mb_h{0.0};
    double      projected_24h_growth_mb{0.0};
    double      queue_depth_pct{0.0};
    std::size_t pool_exhaustions{0};
    uint64_t    queue_rejections{0};
    uint64_t    persistence_overflows{0};
    bool        is_stable{true};
    // Projected hours until threshold breach; -1.0 = stable / not projected
    double      latency_breach_hours{-1.0};
    double      throughput_breach_hours{-1.0};
    double      memory_breach_hours{-1.0};
};

struct CILayerStatus {
    std::string state{"unknown"};  // "unknown" | "passing" | "failing"
    std::string last_run;
    std::string last_failure;
};

struct CISnapshot {
    CILayerStatus layer1;
    CILayerStatus layer2;
    CILayerStatus layer3;
};

// ── Phase 5 snapshot structs ──────────────────────────────────────────────────

struct HardwareSnapshot {
    std::string platform;           // "Linux bare-metal" | "WSL2 (fallbacks active)"
    int         physical_cores{0};
    int         logical_cpus{0};
    bool        hyperthreading{false};
    std::vector<int> isolated_cpus;
    std::size_t l1_kb{0};
    std::size_t l2_kb{0};
    std::size_t l3_kb{0};
    int         numa_nodes_count{0};
    bool        huge_pages_available{false};
    std::size_t huge_page_size_kb{0};
    std::size_t huge_pages_free{0};
    std::size_t huge_pages_total{0};
    std::string simd_level;         // "AVX2" | "AVX-512" | "SSE4.2" | "scalar"
    bool        dpdk_available{false};
};

struct OptGateEntry {
    std::string category;
    double      throughput_gain_pct{0.0};
    double      latency_gain_pct{0.0};
    bool        passed{false};
};

struct OptimizationSnapshot {
    std::vector<std::string> enabled;   // active optimizations
    std::vector<OptGateEntry> results;  // gate results per category
};

struct Phase5BenchRepStats {
    double   mean_throughput{0};
    double   best_throughput{0};
    double   stddev_throughput{0};
    uint64_t mean_p99_ns{0};
    uint64_t best_p99_ns{0};
};

struct Phase5BenchSnapshot {
    // keyed by category name
    std::vector<std::pair<std::string, Phase5BenchRepStats>> categories;
};

// ── EngineSnapshot ────────────────────────────────────────────────────────────

struct EngineSnapshot {
    static constexpr int       MAX_TRADES = 50;
    OrderBookSnapshot          orderbook;
    std::deque<TradeEvent>     recent_trades;
    ReplayStats                replay;
    Metrics                    metrics;
    std::vector<PoolMetrics>   pools;
    StartupMetrics             startup;
    // Phase 4
    BacktestSnapshot           backtest;
    StabilitySnapshot          stability;
    CISnapshot                 ci;
    // Phase 5
    HardwareSnapshot           hardware;
    OptimizationSnapshot       optimization;
    Phase5BenchSnapshot        phase5bench;
    uint64_t                   snapshot_ts{0};
};

} // namespace trading::dashboard
