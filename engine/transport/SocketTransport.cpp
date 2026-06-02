#include "engine/transport/SocketTransport.hpp"
#include <cstring>
#include <iostream>

#ifdef __linux__
#  include <arpa/inet.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace trading {

SocketTransport::SocketTransport(const std::string& address, int port)
    : address_(address), port_(port)
{
    connected_ = open();
    if (connected_)
        std::cout << "[transport] Socket transport ready on "
                  << address_ << ":" << port_ << "\n";
    else
        std::cerr << "[transport] Socket transport init failed — "
                     "send/recv are no-ops\n";
}

SocketTransport::~SocketTransport() { close(); }

bool SocketTransport::open() noexcept {
#ifdef __linux__
    send_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    recv_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_fd_ < 0 || recv_fd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, address_.c_str(), &addr.sin_addr);

    if (bind(recv_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        return false;
    return true;
#else
    return false;
#endif
}

void SocketTransport::close() noexcept {
#ifdef __linux__
    if (send_fd_ >= 0) { ::close(send_fd_); send_fd_ = -1; }
    if (recv_fd_ >= 0) { ::close(recv_fd_); recv_fd_ = -1; }
#endif
    connected_ = false;
}

void SocketTransport::send(const Event* events, std::size_t count) {
    if (!connected_ || !events || count == 0) return;
#ifdef __linux__
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, address_.c_str(), &dest.sin_addr);

    for (std::size_t i = 0; i < count; ++i) {
        ::sendto(send_fd_, &events[i], sizeof(Event), 0,
                 reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    }
#endif
}

std::size_t SocketTransport::recv(Event* out, std::size_t max_count) {
    if (!connected_ || !out || max_count == 0) return 0;
#ifdef __linux__
    std::size_t received = 0;
    while (received < max_count) {
        ssize_t n = ::recv(recv_fd_, &out[received], sizeof(Event),
                           MSG_DONTWAIT);
        if (n == static_cast<ssize_t>(sizeof(Event))) ++received;
        else break;
    }
    return received;
#else
    return 0;
#endif
}

// ── TransportLayer factory ────────────────────────────────────────────────────

std::unique_ptr<TransportLayer> TransportLayer::create(
    Mode mode, const std::string& address, int port)
{
    if (mode == Mode::DPDK) {
        std::cerr << "[transport] DPDK requested — requires bare-metal Linux "
                     "with DPDK NIC. Falling back to socket transport.\n";
    }
    return std::make_unique<SocketTransport>(address, port);
}

} // namespace trading
