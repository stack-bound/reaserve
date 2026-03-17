#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
    inline int close_socket(socket_t s) { return closesocket(s); }
    inline bool init_sockets() {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }
    inline void cleanup_sockets() { WSACleanup(); }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cerrno>
    using socket_t = int;
    constexpr socket_t INVALID_SOCK = -1;
    inline int close_socket(socket_t s) { return close(s); }
    inline bool init_sockets() { return true; }
    inline void cleanup_sockets() {}
#endif
