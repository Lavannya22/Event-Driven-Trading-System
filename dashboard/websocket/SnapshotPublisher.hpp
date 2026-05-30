#pragma once

#include <chrono>
#include <mutex>
#include "dashboard/websocket/Snapshot.hpp"
#include "engine/events/Event.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/replay/ReplayController.hpp"
#include "engine/strategy/StrategyEngine.hpp"

namespace trading::dashboard {

// Thread-safe bridge between engine threads and the WebSocket broadcast thread.
// Engine calls push_* / update_* (brief lock).
// Dashboard server calls get_snapshot() to read a consistent copy.
class SnapshotPublisher {
public:
    void update_orderbook(const OrderBook& book) {
        auto bids = book.top_bids(10);
        auto asks = book.top_asks(10);
        std::lock_guard lock(mu_);
        auto& ob    = snap_.orderbook;
        ob.symbol_id = book.symbol_id();
        ob.best_bid  = book.has_bid() ? book.best_bid() : 0;
        ob.best_ask  = book.has_ask() ? book.best_ask() : 0;
        ob.spread    = book.spread();
        ob.bids.clear();
        for (auto& d : bids) ob.bids.push_back({d.price, d.quantity});
        ob.asks.clear();
        for (auto& d : asks) ob.asks.push_back({d.price, d.quantity});
    }

    void push_trade(const Event& e) {
        TradeEvent t{e.timestamp, e.symbol_id, e.order_id,
                     e.price, e.quantity,
                     static_cast<uint8_t>(event_side(e))};
        std::lock_guard lock(mu_);
        snap_.recent_trades.push_back(t);
        while (snap_.recent_trades.size() > static_cast<std::size_t>(EngineSnapshot::MAX_TRADES))
            snap_.recent_trades.pop_front();
    }

    void update_replay(const ReplayController& rc) {
        std::lock_guard lock(mu_);
        snap_.replay.events_replayed = rc.events_replayed();
        snap_.replay.done            = rc.done();
        snap_.replay.source_name     = rc.source_name();
    }

    void update_replay_timestamp(uint64_t ts) {
        std::lock_guard lock(mu_);
        snap_.replay.last_timestamp = ts;
    }

    void update_metrics(const StrategyEngine& se,
                        double queue_occupancy,
                        double throughput_eps,
                        double avg_latency_us) {
        std::lock_guard lock(mu_);
        snap_.metrics.signals         = se.signal_count();
        snap_.metrics.noops           = se.noop_count();
        snap_.metrics.queue_occupancy = queue_occupancy;
        snap_.metrics.throughput_eps  = throughput_eps;
        snap_.metrics.avg_latency_us  = avg_latency_us;
    }

    EngineSnapshot get_snapshot() {
        std::lock_guard lock(mu_);
        snap_.snapshot_ts = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        return snap_;
    }

private:
    std::mutex     mu_;
    EngineSnapshot snap_;
};

} // namespace trading::dashboard
