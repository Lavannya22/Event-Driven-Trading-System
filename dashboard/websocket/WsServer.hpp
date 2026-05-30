#pragma once

#include <memory>
#include <string>

namespace trading::dashboard {

// Minimal RFC-6455 WebSocket push server (server→client text frames only).
// Accepts multiple clients concurrently; broadcast() is thread-safe.
// No external dependencies — uses POSIX sockets and pthreads.
class WsServer {
public:
    explicit WsServer(int port = 9001);
    ~WsServer();

    bool start();                           // bind, listen, spawn accept thread
    void stop();                            // graceful shutdown; blocks until done
    void broadcast(const std::string& msg); // send text frame to every connected client
    int  client_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace trading::dashboard
