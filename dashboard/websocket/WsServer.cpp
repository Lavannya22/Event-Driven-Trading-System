#include "dashboard/websocket/WsServer.hpp"
#include "dashboard/websocket/sha1.hpp"
#include "dashboard/websocket/base64.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace trading::dashboard {

static constexpr const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static std::string extract_ws_key(const std::string& req) {
    std::string lower = req;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const std::string marker = "sec-websocket-key:";
    auto pos = lower.find(marker);
    if (pos == std::string::npos) return {};
    pos += marker.size();
    while (pos < req.size() && req[pos] == ' ') ++pos;
    auto end = req.find("\r\n", pos);
    if (end == std::string::npos) end = req.size();
    return req.substr(pos, end - pos);
}

static std::string compute_accept(const std::string& key) {
    std::string combined = key + WS_GUID;
    auto hash = sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size());
    return base64_encode(hash.data(), 20);
}

static std::vector<uint8_t> text_frame(const std::string& msg) {
    std::vector<uint8_t> f;
    std::size_t n = msg.size();
    f.push_back(0x81u); // FIN + text opcode
    if (n < 126) {
        f.push_back(static_cast<uint8_t>(n));
    } else if (n < 65536) {
        f.push_back(126u);
        f.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
        f.push_back(static_cast<uint8_t>( n       & 0xFF));
    } else {
        f.push_back(127u);
        for (int i = 7; i >= 0; --i)
            f.push_back(static_cast<uint8_t>((n >> (i * 8)) & 0xFF));
    }
    f.insert(f.end(), msg.begin(), msg.end());
    return f;
}

struct WsServer::Impl {
    int              port_;
    int              listen_fd_{-1};
    std::atomic_bool running_{false};
    std::thread      accept_thread_;
    mutable std::mutex  mtx_;
    std::vector<int> clients_;

    explicit Impl(int port) : port_(port) {}
    ~Impl() { stop(); }

    bool start() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return false;

        int opt = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(static_cast<uint16_t>(port_));

        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
            ::listen(listen_fd_, 16) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        running_ = true;
        accept_thread_ = std::thread([this] { accept_loop(); });
        return true;
    }

    void stop() {
        running_ = false;
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        if (accept_thread_.joinable()) accept_thread_.join();
        std::lock_guard<std::mutex> lk(mtx_);
        for (int fd : clients_) ::close(fd);
        clients_.clear();
    }

    void accept_loop() {
        while (running_) {
            pollfd pfd{listen_fd_, POLLIN, 0};
            if (::poll(&pfd, 1, 200) <= 0) continue;
            sockaddr_in ca{};
            socklen_t   cl = sizeof(ca);
            int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&ca), &cl);
            if (fd < 0) continue;
            std::thread([this, fd] { handshake(fd); }).detach();
        }
    }

    void handshake(int fd) {
        std::string req;
        char buf[2048];
        while (req.find("\r\n\r\n") == std::string::npos) {
            int n = static_cast<int>(::recv(fd, buf, sizeof(buf) - 1, 0));
            if (n <= 0) { ::close(fd); return; }
            buf[n] = '\0';
            req.append(buf, static_cast<std::size_t>(n));
            if (req.size() > 8192) { ::close(fd); return; }
        }

        std::string key = extract_ws_key(req);
        if (key.empty()) { ::close(fd); return; }

        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + compute_accept(key) + "\r\n\r\n";

        ssize_t sent = ::send(fd, resp.data(), resp.size(), 0);
        if (sent < 0 || static_cast<std::size_t>(sent) != resp.size()) {
            ::close(fd);
            return;
        }

        { std::lock_guard<std::mutex> lk(mtx_); clients_.push_back(fd); }
        std::cout << "[ws] client connected (total=" << client_count() << ")\n";

        // Drain incoming frames to detect disconnect; ignore content.
        char discard[64];
        while (running_) {
            pollfd pfd{fd, POLLIN | POLLHUP | POLLERR, 0};
            if (::poll(&pfd, 1, 200) <= 0) continue;
            if (pfd.revents & (POLLHUP | POLLERR)) break;
            if (::recv(fd, discard, sizeof(discard), 0) <= 0) break;
        }

        { std::lock_guard<std::mutex> lk(mtx_);
          clients_.erase(std::remove(clients_.begin(), clients_.end(), fd), clients_.end()); }
        ::close(fd);
        std::cout << "[ws] client disconnected (total=" << client_count() << ")\n";
    }

    void broadcast(const std::string& msg) {
        auto frame = text_frame(msg);
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<int> dead;
        for (int fd : clients_) {
            if (::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) < 0)
                dead.push_back(fd);
        }
        for (int fd : dead) {
            clients_.erase(std::remove(clients_.begin(), clients_.end(), fd), clients_.end());
            ::close(fd);
        }
    }

    int client_count() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return static_cast<int>(clients_.size());
    }
};

WsServer::WsServer(int port) : impl_(std::make_unique<Impl>(port)) {}
WsServer::~WsServer() = default;
bool WsServer::start()  { return impl_->start(); }
void WsServer::stop()   { impl_->stop(); }
void WsServer::broadcast(const std::string& msg) { impl_->broadcast(msg); }
int  WsServer::client_count() const { return impl_->client_count(); }

} // namespace trading::dashboard
