#include "dashboard/websocket/DashboardServer.hpp"

// Pull in the full uWebSockets + nlohmann headers only in the .cpp.
#include <App.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace trading::dashboard {

namespace {
struct PerSocketData {};
using WS = uWS::WebSocket<false, true, PerSocketData>;
} // namespace

// ---------------------------------------------------------------------------
DashboardServer::DashboardServer(SnapshotPublisher& publisher,
                                 int port,
                                 int broadcast_interval_ms)
    : publisher_(publisher), port_(port), interval_ms_(broadcast_interval_ms)
{}

DashboardServer::~DashboardServer() { stop(); }

// ---------------------------------------------------------------------------
void DashboardServer::run() {
    running_.store(true);

    uWS::App app;
    loop_          = uWS::Loop::get();

    app.ws<PerSocketData>("/*", {
        .compression      = uWS::DISABLED,
        .maxPayloadLength = 64 * 1024,
        .idleTimeout      = 0,
        .open = [](WS* ws) {
            ws->subscribe("snap");
        },
        .message = [](WS*, std::string_view, uWS::OpCode) {
            // Dashboard is read-only in Phase 1; ignore client messages.
        },
        .close = [](WS*, int, std::string_view) {}
    })
    .listen(port_, [this](us_listen_socket_t* token) {
        listen_socket_ = token;
        if (token) {
            std::cout << "[dashboard] WebSocket server listening on ws://localhost:"
                      << port_ << "\n";
        } else {
            std::cerr << "[dashboard] Failed to bind port " << port_ << "\n";
        }
    });

    // Broadcast thread: sleep → build JSON → defer publish to event loop.
    std::thread bcast([this, &app]() {
        while (running_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
            if (!running_.load(std::memory_order_relaxed)) break;

            std::string json = to_json(publisher_.get_snapshot());
            loop_->defer([&app, j = std::move(json)]() mutable {
                app.publish("snap", std::move(j), uWS::OpCode::TEXT);
            });
        }
        // Close the listen socket on the event loop thread so app.run() returns.
        loop_->defer([this]() {
            if (listen_socket_) {
                us_listen_socket_close(0, listen_socket_);
                listen_socket_ = nullptr;
            }
        });
    });

    app.run();  // blocks until listen socket is closed
    bcast.join();
}

// ---------------------------------------------------------------------------
void DashboardServer::stop() noexcept {
    if (!running_.exchange(false)) return;
    // If the event loop is running, terminate it directly so stop() is
    // responsive even if the broadcast thread is sleeping.
    if (loop_) loop_->defer([this]() {
        if (listen_socket_) {
            us_listen_socket_close(0, listen_socket_);
            listen_socket_ = nullptr;
        }
    });
}

// ---------------------------------------------------------------------------
std::string DashboardServer::to_json(const EngineSnapshot& snap) {
    using json = nlohmann::json;

    // --- order book ---
    json ob;
    ob["symbol_id"] = snap.orderbook.symbol_id;
    ob["best_bid"]  = snap.orderbook.best_bid;
    ob["best_ask"]  = snap.orderbook.best_ask;
    ob["spread"]    = snap.orderbook.spread;
    ob["bids"]      = json::array();
    for (auto& d : snap.orderbook.bids)
        ob["bids"].push_back({{"price", d.price}, {"qty", d.quantity}});
    ob["asks"] = json::array();
    for (auto& d : snap.orderbook.asks)
        ob["asks"].push_back({{"price", d.price}, {"qty", d.quantity}});

    // --- recent trades ---
    json trades = json::array();
    for (auto& t : snap.recent_trades)
        trades.push_back({{"ts", t.timestamp}, {"symbol_id", t.symbol_id},
                          {"order_id", t.order_id}, {"price", t.price},
                          {"qty", t.quantity}, {"side", t.side}});

    // --- replay ---
    json replay = {
        {"source",          snap.replay.source_name},
        {"events_replayed", snap.replay.events_replayed},
        {"last_ts",         snap.replay.last_timestamp},
        {"done",            snap.replay.done}
    };

    // --- metrics ---
    json metrics = {
        {"throughput_eps",         snap.metrics.throughput_eps},
        {"queue_occupancy",        snap.metrics.queue_occupancy},
        {"avg_latency_us",         snap.metrics.avg_latency_us},
        // Live engine latency histogram
        {"p50_ns",                 snap.metrics.p50_ns},
        {"p99_ns",                 snap.metrics.p99_ns},
        {"p999_ns",                snap.metrics.p999_ns},
        {"max_ns",                 snap.metrics.max_ns},
        // Tick-to-trade
        {"tick_to_trade_p50_ns",   snap.metrics.tick_to_trade_p50_ns},
        {"tick_to_trade_p99_ns",   snap.metrics.tick_to_trade_p99_ns},
        {"tick_to_trade_max_ns",   snap.metrics.tick_to_trade_max_ns},
        {"signals",                snap.metrics.signals},
        {"noops",                  snap.metrics.noops}
    };

    // --- pool utilisation ---
    json pools = json::array();
    for (const auto& p : snap.pools)
        pools.push_back({{"name",             p.name},
                         {"capacity",         p.capacity},
                         {"used",             p.used},
                         {"available",        p.available},
                         {"exhaustion_count", p.exhaustion_count}});

    // --- startup report ---
    json startup = {
        {"allocator_started",       snap.startup.allocator_started},
        {"mlockall_success",        snap.startup.mlockall_success},
        {"event_pool_capacity",     snap.startup.event_pool_capacity},
        {"order_pool_capacity",     snap.startup.order_pool_capacity},
        {"execution_pool_capacity", snap.startup.execution_pool_capacity},
        {"signal_pool_capacity",    snap.startup.signal_pool_capacity},
        {"validation_passed",       snap.startup.validation_passed}
    };

    // --- backtest (Phase 4) ---
    json bt_results = json::array();
    for (const auto& r : snap.backtest.results)
        bt_results.push_back({
            {"run_number",   r.run_number},
            {"window_size",  r.window_size},
            {"threshold",    r.threshold},
            {"total_trades", r.total_trades},
            {"pnl",          r.realized_pnl},
            {"sharpe",       r.sharpe_ratio},
            {"win_rate",     r.win_rate},
            {"max_drawdown", r.max_drawdown}
        });
    json backtest = {
        {"results",     std::move(bt_results)},
        {"running",     snap.backtest.running},
        {"current_run", snap.backtest.current_run},
        {"total_runs",  snap.backtest.total_runs}
    };

    // --- stability (Phase 4) ---
    const auto& st = snap.stability;
    json stability = {
        {"latency_drift_pct",        st.latency_drift_pct},
        {"throughput_drift_pct",     st.throughput_drift_pct},
        {"memory_current_mb",        st.memory_current_mb},
        {"memory_peak_mb",           st.memory_peak_mb},
        {"memory_growth_rate_mb_h",  st.memory_growth_rate_mb_h},
        {"projected_24h_growth_mb",  st.projected_24h_growth_mb},
        {"queue_depth_pct",          st.queue_depth_pct},
        {"pool_exhaustions",         st.pool_exhaustions},
        {"queue_rejections",         st.queue_rejections},
        {"persistence_overflows",    st.persistence_overflows},
        {"is_stable",                st.is_stable},
        {"latency_breach_hours",     st.latency_breach_hours},
        {"throughput_breach_hours",  st.throughput_breach_hours},
        {"memory_breach_hours",      st.memory_breach_hours}
    };

    // --- CI status (Phase 4) ---
    auto ci_layer = [](const CILayerStatus& l) {
        return json{
            {"state",        l.state},
            {"last_run",     l.last_run},
            {"last_failure", l.last_failure}
        };
    };
    json ci = {
        {"layer1", ci_layer(snap.ci.layer1)},
        {"layer2", ci_layer(snap.ci.layer2)},
        {"layer3", ci_layer(snap.ci.layer3)}
    };

    // --- hardware (Phase 5) ---
    const auto& hw = snap.hardware;
    json iso_cpus = json::array();
    for (int c : hw.isolated_cpus) iso_cpus.push_back(c);
    json hardware = {
        {"platform",             hw.platform},
        {"physical_cores",       hw.physical_cores},
        {"logical_cpus",         hw.logical_cpus},
        {"hyperthreading",       hw.hyperthreading},
        {"isolated_cpus",        std::move(iso_cpus)},
        {"l1_kb",                hw.l1_kb},
        {"l2_kb",                hw.l2_kb},
        {"l3_kb",                hw.l3_kb},
        {"numa_nodes",           hw.numa_nodes_count},
        {"huge_pages_available", hw.huge_pages_available},
        {"huge_page_size_kb",    hw.huge_page_size_kb},
        {"huge_pages_free",      hw.huge_pages_free},
        {"huge_pages_total",     hw.huge_pages_total},
        {"simd_level",           hw.simd_level},
        {"dpdk_available",       hw.dpdk_available}
    };

    // --- optimization (Phase 5) ---
    json opt_enabled = json::array();
    for (const auto& e : snap.optimization.enabled) opt_enabled.push_back(e);
    json opt_results = json::array();
    for (const auto& r : snap.optimization.results)
        opt_results.push_back({
            {"category",              r.category},
            {"throughput_gain_pct",   r.throughput_gain_pct},
            {"latency_gain_pct",      r.latency_gain_pct},
            {"passed",                r.passed}
        });
    json optimization = {
        {"enabled", std::move(opt_enabled)},
        {"results", std::move(opt_results)}
    };

    // --- phase5bench (Phase 5) ---
    json p5cats = json::object();
    for (const auto& [name, s] : snap.phase5bench.categories)
        p5cats[name] = {
            {"mean_throughput",  s.mean_throughput},
            {"best_throughput",  s.best_throughput},
            {"stddev_throughput",s.stddev_throughput},
            {"mean_p99_ns",      s.mean_p99_ns},
            {"best_p99_ns",      s.best_p99_ns}
        };
    json phase5bench = {{"categories", std::move(p5cats)}};

    return json{
        {"type",        "snapshot"},
        {"ts",          snap.snapshot_ts},
        {"orderbook",   std::move(ob)},
        {"trades",      std::move(trades)},
        {"replay",      std::move(replay)},
        {"metrics",     std::move(metrics)},
        {"pools",       std::move(pools)},
        {"startup",     std::move(startup)},
        {"backtest",    std::move(backtest)},
        {"stability",   std::move(stability)},
        {"ci",          std::move(ci)},
        {"hardware",    std::move(hardware)},
        {"optimization",std::move(optimization)},
        {"phase5bench", std::move(phase5bench)}
    }.dump();
}

} // namespace trading::dashboard
