#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "benchmarks/LatencyHistogram.hpp"
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
// Each cycle:
//   1. Builds a full book (6 bid levels + 6 ask levels around mid=10000)
//   2. Executes two crossing trades so fills appear in the Trades tab
//   3. The replay controller loops so the book stays live indefinitely.
// ---------------------------------------------------------------------------
static std::vector<Event> make_demo_events() {
    std::vector<Event> ev;
    uint64_t ts  = 1'000'000;   // nanoseconds
    uint64_t oid = 1;

    auto bid = [&](uint64_t price, uint64_t qty) {
        ev.push_back(make_new_order(ts, 1, oid++, price, qty, Side::Bid));
        ts += 100'000;
    };
    auto ask = [&](uint64_t price, uint64_t qty) {
        ev.push_back(make_new_order(ts, 1, oid++, price, qty, Side::Ask));
        ts += 100'000;
    };

    // ── Resting bids (6 levels, best=9999) ───────────────────────────────
    bid(9999, 120);
    bid(9997, 200);
    bid(9995, 180);
    bid(9990, 300);
    bid(9985, 250);
    bid(9980, 400);

    // ── Resting asks (6 levels, best=10001) ──────────────────────────────
    ask(10001, 150);
    ask(10003, 220);
    ask(10005, 180);
    ask(10010, 300);
    ask(10015, 250);
    ask(10020, 350);

    // ── Aggressive bid: buys the best ask (partial fill → ask remains) ───
    bid(10001, 60);   // fills 60 of ask@10001 (90 left)

    // ── Aggressive ask: sells to the best bid (partial fill → bid remains)
    ask(9999, 50);    // fills 50 of bid@9999 (70 left)

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
        using Clock = std::chrono::steady_clock;
        using ns    = std::chrono::nanoseconds;

        auto now_ns = []() -> uint64_t {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<ns>(
                    Clock::now().time_since_epoch()).count());
        };

        Event    event{};
        uint64_t processed = 0;
        double   running_latency_us = 0.0;
        auto     t_start = Clock::now();

        // Live histograms — reset every 10 000 events to stay fresh.
        bench::LatencyHistogram engine_hist;      // event-arrival → match complete
        bench::LatencyHistogram t2t_hist;         // event-arrival → trade produced
        constexpr uint64_t HIST_RESET_INTERVAL = 10'000;

        while (!g_stop.load() && rc->next(event)) {
            publisher.update_replay(*rc);
            publisher.update_replay_timestamp(event.timestamp);

            // t_recv: full hot-path start — includes strategy dispatch.
            const uint64_t t_recv = now_ns();

            // Strategy gate
            const auto sig = strategy.process(event);
            if (sig == StrategySignal::Noop) continue;

            // Route to matching engine
            bool produced_trade = false;
            const auto etype = event_type(event);
            if (etype == EventType::NewOrder) {
                auto out = matcher.process_new_order(book, event);
                for (auto& e : out.event_span()) {
                    if (event_type(e) == EventType::TradeExecution) {
                        publisher.push_trade(e);
                        produced_trade = true;
                    }
                }
            } else if (etype == EventType::CancelOrder) {
                matcher.process_cancel(book, event);
            } else if (etype == EventType::ModifyOrder) {
                matcher.process_modify(book, event);
            }

            const uint64_t t_done = now_ns();
            const uint64_t lat_ns = t_done - t_recv;

            // Engine latency histogram (every signalled event).
            engine_hist.record(lat_ns);

            // Tick-to-trade histogram (only events that produced fills).
            if (produced_trade)
                t2t_hist.record(lat_ns);

            publisher.update_orderbook(book);

            // Metrics
            ++processed;
            const double lat_us = static_cast<double>(lat_ns) * 1e-3;
            running_latency_us = (processed == 1)
                ? lat_us
                : 0.9 * running_latency_us + 0.1 * lat_us;

            const double elapsed_s =
                std::chrono::duration<double>(Clock::now() - t_start).count();
            const double throughput =
                elapsed_s > 0.0 ? static_cast<double>(processed) / elapsed_s : 0.0;

            publisher.update_metrics(strategy, 0.0, throughput, running_latency_us);

            // Publish histogram percentiles every HIST_RESET_INTERVAL events.
            if (processed % HIST_RESET_INTERVAL == 0) {
                publisher.update_latency_histogram(
                    engine_hist.p50(), engine_hist.p99(),
                    engine_hist.p999(), engine_hist.max_ns());
                publisher.update_tick_to_trade(
                    t2t_hist.p50(), t2t_hist.p99(), t2t_hist.max_ns());
                engine_hist.reset();
                t2t_hist.reset();
            }
        }

        // Final publish of whatever is in the histogram.
        publisher.update_latency_histogram(
            engine_hist.p50(), engine_hist.p99(),
            engine_hist.p999(), engine_hist.max_ns());
        publisher.update_tick_to_trade(
            t2t_hist.p50(), t2t_hist.p99(), t2t_hist.max_ns());

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
