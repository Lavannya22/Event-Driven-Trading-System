#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "engine/events/Event.hpp"
#include "engine/matching/MatchingEngine.hpp"
#include "engine/orderbook/OrderBook.hpp"
#include "engine/replay/ReplayController.hpp"
#include "engine/replay/SequentialReplayController.hpp"
#include "engine/replay/VectorReplayController.hpp"
#include "engine/strategy/Strategy.hpp"
#include "engine/strategy/StrategyEngine.hpp"
#include "dashboard/websocket/DashboardServer.hpp"
#include "dashboard/websocket/SnapshotPublisher.hpp"

using namespace trading;
using namespace trading::dashboard;

// ---------------------------------------------------------------------------
// Demo event set — used when no CSV file is supplied.
// Generates a small book with crossing orders to produce visible fills.
// ---------------------------------------------------------------------------
static std::vector<Event> make_demo_events() {
    std::vector<Event> ev;

    // Resting asks
    ev.push_back(make_new_order(1000, 1, 101, 10060, 100, Side::Ask));
    ev.push_back(make_new_order(1001, 1, 102, 10070, 200, Side::Ask));
    ev.push_back(make_new_order(1002, 1, 103, 10080, 150, Side::Ask));

    // Resting bids
    ev.push_back(make_new_order(1003, 1, 201, 10050, 100, Side::Bid));
    ev.push_back(make_new_order(1004, 1, 202, 10040, 250, Side::Bid));

    // Aggressive bids that cross the spread and produce fills
    ev.push_back(make_new_order(2000, 1, 301, 10060, 50,  Side::Bid));  // partial fill
    ev.push_back(make_new_order(2001, 1, 302, 10065, 200, Side::Bid));  // crosses 10060 entirely

    // Aggressive asks
    ev.push_back(make_new_order(3000, 1, 401, 10050, 100, Side::Ask));  // fills 201

    // Cancel a resting order
    ev.push_back(make_cancel_order(4000, 1, 103));

    return ev;
}

// ---------------------------------------------------------------------------
// Signal handling
// ---------------------------------------------------------------------------
static std::atomic<bool>  g_stop{false};
static DashboardServer*   g_server{nullptr};

static void on_signal(int) {
    g_stop.store(true);
    if (g_server) g_server->stop();
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // --- engine setup ---
    OrderBook       book(1);
    MatchingEngine  matcher;
    StrategyEngine  strategy(std::make_unique<PassThroughStrategy>());
    SnapshotPublisher publisher;

    // --- replay source ---
    std::unique_ptr<ReplayController> rc;
    if (argc > 1) {
        try {
            rc = std::make_unique<SequentialReplayController>(argv[1]);
            std::cout << "[dashboard] Replaying: " << argv[1] << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "[dashboard] Cannot open CSV: " << ex.what() << "\n";
            return EXIT_FAILURE;
        }
    } else {
        rc = std::make_unique<VectorReplayController>(make_demo_events(), "demo");
        std::cout << "[dashboard] No CSV supplied — running built-in demo events.\n";
    }

    // --- dashboard server ---
    DashboardServer server(publisher, 9001, 200);
    g_server = &server;
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // --- replay thread ---
    std::thread replay_thread([&]() {
        Event    event{};
        uint64_t processed = 0;
        double   running_latency_us = 0.0;
        auto     t_start = std::chrono::steady_clock::now();

        while (!g_stop.load() && rc->next(event)) {
            publisher.update_replay(*rc);
            publisher.update_replay_timestamp(event.timestamp);

            // Strategy gate
            auto sig = strategy.process(event);
            if (sig == StrategySignal::Noop) continue;

            // Time only the core engine path (strategy already ran above)
            auto t0 = std::chrono::steady_clock::now();

            // Route to matching engine
            auto etype = event_type(event);
            if (etype == EventType::NewOrder) {
                auto out = matcher.process_new_order(book, event);
                for (auto& e : out.event_span()) {
                    if (event_type(e) == EventType::TradeExecution)
                        publisher.push_trade(e);
                }
            } else if (etype == EventType::CancelOrder) {
                matcher.process_cancel(book, event);
            } else if (etype == EventType::ModifyOrder) {
                matcher.process_modify(book, event);
            }

            auto t1 = std::chrono::steady_clock::now();
            double lat_us = std::chrono::duration<double>(t1 - t0).count() * 1e6;

            publisher.update_orderbook(book);

            // Metrics
            ++processed;
            // Exponential moving average for latency
            running_latency_us = (processed == 1)
                ? lat_us
                : 0.9 * running_latency_us + 0.1 * lat_us;

            double elapsed_s = std::chrono::duration<double>(t1 - t_start).count();
            double throughput = elapsed_s > 0.0 ? processed / elapsed_s : 0.0;
            publisher.update_metrics(strategy, 0.0, throughput, running_latency_us);
        }

        // Final replay update (marks done=true)
        publisher.update_replay(*rc);

        if (!g_stop.load()) {
            std::cout << "[dashboard] Replay complete — "
                      << rc->events_replayed() << " events. "
                      << "Server stays up. Press Ctrl+C to exit.\n";
        }
    });

    // --- run WebSocket server on this thread (blocks) ---
    server.run();

    replay_thread.join();
    std::cout << "[dashboard] Shutdown complete.\n";
    return EXIT_SUCCESS;
}
