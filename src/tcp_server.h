#pragma once

#include "platform.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <cstdint>

namespace reaserve {

// Length-prefixed framing: 4-byte big-endian uint32 length + payload
namespace framing {
    std::string encode(const std::string& payload);
    // Read exactly n bytes from socket. Returns false on error/disconnect.
    bool recv_exact(socket_t sock, char* buf, size_t n);
    // Read one framed message. Returns false on error/disconnect.
    bool read_message(socket_t sock, std::string& out);
    // Write one framed message. Returns false on error.
    bool write_message(socket_t sock, const std::string& payload);
}

using RequestHandler = std::function<std::string(const std::string& request)>;

class TcpServer {
public:
    TcpServer(const std::string& bind_addr, int port, RequestHandler handler);
    ~TcpServer();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void accept_loop();
    void client_loop(socket_t client_sock);

    std::string bind_addr_;
    int port_;
    RequestHandler handler_;
    std::atomic<bool> running_{false};
    socket_t listen_sock_ = INVALID_SOCK;
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex clients_mutex_;
};

} // namespace reaserve
