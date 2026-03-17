#include "tcp_server.h"
#include <cstring>
#include <algorithm>

namespace reaserve {
namespace framing {

std::string encode(const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    std::string frame(4 + payload.size(), '\0');
    frame[0] = static_cast<char>((len >> 24) & 0xFF);
    frame[1] = static_cast<char>((len >> 16) & 0xFF);
    frame[2] = static_cast<char>((len >> 8) & 0xFF);
    frame[3] = static_cast<char>(len & 0xFF);
    std::memcpy(&frame[4], payload.data(), payload.size());
    return frame;
}

bool recv_exact(socket_t sock, char* buf, size_t n) {
    size_t received = 0;
    while (received < n) {
        auto r = recv(sock, buf + received, static_cast<int>(n - received), 0);
        if (r <= 0) return false;
        received += static_cast<size_t>(r);
    }
    return true;
}

bool read_message(socket_t sock, std::string& out) {
    char header[4];
    if (!recv_exact(sock, header, 4)) return false;

    uint32_t len = (static_cast<uint8_t>(header[0]) << 24) |
                   (static_cast<uint8_t>(header[1]) << 16) |
                   (static_cast<uint8_t>(header[2]) << 8) |
                   static_cast<uint8_t>(header[3]);

    if (len == 0 || len > 10 * 1024 * 1024) return false; // Max 10MB

    out.resize(len);
    return recv_exact(sock, &out[0], len);
}

bool write_message(socket_t sock, const std::string& payload) {
    std::string frame = encode(payload);
    size_t sent = 0;
    while (sent < frame.size()) {
        auto s = send(sock, frame.data() + sent, static_cast<int>(frame.size() - sent), 0);
        if (s <= 0) return false;
        sent += static_cast<size_t>(s);
    }
    return true;
}

} // namespace framing

TcpServer::TcpServer(const std::string& bind_addr, int port, RequestHandler handler)
    : bind_addr_(bind_addr), port_(port), handler_(std::move(handler)) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (!init_sockets()) return false;

    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == INVALID_SOCK) return false;

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr);

    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    if (listen(listen_sock_, 5) != 0) {
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&TcpServer::accept_loop, this);
    return true;
}

void TcpServer::stop() {
    running_ = false;
    if (listen_sock_ != INVALID_SOCK) {
        shutdown(listen_sock_, SHUT_RDWR_COMPAT);
        close_socket(listen_sock_);
        listen_sock_ = INVALID_SOCK;
    }

    // Shutdown all active client sockets so their blocking recv() calls
    // return immediately instead of hanging until the remote end closes.
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto sock : active_client_sockets_) {
            shutdown(sock, SHUT_RDWR_COMPAT);
            close_socket(sock);
        }
        active_client_sockets_.clear();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& t : client_threads_) {
        if (t.joinable()) t.join();
    }
    client_threads_.clear();
    cleanup_sockets();
}

void TcpServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        socket_t client = accept(listen_sock_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client == INVALID_SOCK) {
            continue; // Server shutting down or error
        }

        std::lock_guard<std::mutex> lock(clients_mutex_);
        // Clean up finished threads
        client_threads_.erase(
            std::remove_if(client_threads_.begin(), client_threads_.end(),
                [](std::thread& t) {
                    if (t.joinable()) {
                        // We can't easily check if a thread is done without joining,
                        // so we just keep them until stop(). This is fine for a small
                        // number of connections.
                        return false;
                    }
                    return true;
                }),
            client_threads_.end()
        );
        active_client_sockets_.push_back(client);
        client_threads_.emplace_back(&TcpServer::client_loop, this, client);
    }
}

void TcpServer::client_loop(socket_t client_sock) {
    while (running_) {
        std::string request;
        if (!framing::read_message(client_sock, request)) break;

        std::string response = handler_(request);
        if (!framing::write_message(client_sock, response)) break;
    }
    close_socket(client_sock);

    // Remove from active set
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        active_client_sockets_.erase(
            std::remove(active_client_sockets_.begin(), active_client_sockets_.end(), client_sock),
            active_client_sockets_.end());
    }
}

} // namespace reaserve
