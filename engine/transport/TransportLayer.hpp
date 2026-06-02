#pragma once

#include "engine/events/Event.hpp"
#include <cstddef>
#include <memory>
#include <string>

namespace trading {

// Abstract transport interface for receiving market event streams.
// Phase 5 adds two implementations:
//   SocketTransport — standard UDP/TCP socket (always available)
//   DpdkTransport   — kernel-bypass via DPDK (Linux bare-metal only)
//
// Startup options:
//   --transport=socket  (default)
//   --transport=dpdk
//
// DPDK is primarily beneficial for live market data ingestion.
// Replay workloads use file-based ReplayController instead.
class TransportLayer {
public:
    enum class Mode { Socket, DPDK };

    virtual ~TransportLayer() = default;

    // Send a batch of events to connected subscribers (non-blocking).
    virtual void send(const Event* events, std::size_t count) = 0;

    // Receive up to max_count events into `out`. Returns events received.
    virtual std::size_t recv(Event* out, std::size_t max_count) = 0;

    virtual bool        connected() const noexcept = 0;
    virtual const char* mode_name() const noexcept = 0;

    // Factory — returns the requested mode or falls back to Socket on failure.
    static std::unique_ptr<TransportLayer> create(
        Mode mode, const std::string& address = "127.0.0.1", int port = 9002);
};

} // namespace trading
