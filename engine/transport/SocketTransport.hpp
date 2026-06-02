#pragma once

#include "engine/transport/TransportLayer.hpp"
#include <string>

namespace trading {

// UDP socket transport — portable fallback used on WSL2, Docker, and
// any system where DPDK is unavailable.
class SocketTransport final : public TransportLayer {
public:
    SocketTransport(const std::string& address, int port);
    ~SocketTransport() override;

    void        send(const Event* events, std::size_t count) override;
    std::size_t recv(Event* out, std::size_t max_count) override;
    bool        connected() const noexcept override { return connected_; }
    const char* mode_name() const noexcept override { return "socket"; }

private:
    std::string address_;
    int         port_;
    int         send_fd_{-1};
    int         recv_fd_{-1};
    bool        connected_{false};

    bool open() noexcept;
    void close() noexcept;
};

} // namespace trading
