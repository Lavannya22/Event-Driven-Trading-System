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
        {"throughput_eps",  snap.metrics.throughput_eps},
        {"queue_occupancy", snap.metrics.queue_occupancy},
        {"avg_latency_us",  snap.metrics.avg_latency_us},
        {"signals",         snap.metrics.signals},
        {"noops",           snap.metrics.noops}
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

    return json{
        {"type",      "snapshot"},
        {"ts",        snap.snapshot_ts},
        {"orderbook", std::move(ob)},
        {"trades",    std::move(trades)},
        {"replay",    std::move(replay)},
        {"metrics",   std::move(metrics)},
        {"pools",     std::move(pools)},
        {"startup",   std::move(startup)}
    }.dump();
}

} // namespace trading::dashboard
