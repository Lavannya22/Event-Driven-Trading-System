#pragma once

#include <atomic>
#include <string>
#include "dashboard/websocket/Snapshot.hpp"
#include "dashboard/websocket/SnapshotPublisher.hpp"

// Forward-declare uWS types to avoid pulling the full header into this interface.
namespace uWS { struct Loop; }
struct us_listen_socket_t;

namespace trading::dashboard {

// WebSocket server (uWebSockets) that pushes JSON engine snapshots to
// connected React clients every `broadcast_interval_ms` milliseconds.
//
// Architecture: Engine → SnapshotPublisher → DashboardServer → browser.
// The server NEVER touches engine internals directly.
class DashboardServer {
public:
    explicit DashboardServer(SnapshotPublisher& publisher,
                             int port                = 9001,
                             int broadcast_interval_ms = 200);
    ~DashboardServer();

    // Starts the broadcast thread and runs the uWebSockets event loop.
    // Blocks until stop() is called.
    void run();

    // Signal the server to shut down. Safe to call from any thread.
    void stop() noexcept;

private:
    static std::string to_json(const EngineSnapshot& snap);

    SnapshotPublisher&       publisher_;
    int                      port_;
    int                      interval_ms_;
    std::atomic<bool>        running_{false};
    uWS::Loop*               loop_{nullptr};
    us_listen_socket_t*      listen_socket_{nullptr};
};

} // namespace trading::dashboard
