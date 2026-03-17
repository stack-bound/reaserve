#include "tcp_server.h"
#include <cassert>
#include <iostream>
#include <cstring>
#include <csignal>
#include <vector>
#include <thread>

using namespace reaserve;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::cout << "  " << #name << "... "; \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        std::cout << "PASS" << std::endl; \
    } while(0)

void test_encode_empty() {
    TEST(encode_empty);
    auto frame = framing::encode("");
    assert(frame.size() == 4);
    assert(frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 0);
    PASS();
}

void test_encode_small() {
    TEST(encode_small);
    std::string payload = "hello";
    auto frame = framing::encode(payload);
    assert(frame.size() == 4 + 5);
    assert(static_cast<uint8_t>(frame[0]) == 0);
    assert(static_cast<uint8_t>(frame[1]) == 0);
    assert(static_cast<uint8_t>(frame[2]) == 0);
    assert(static_cast<uint8_t>(frame[3]) == 5);
    assert(frame.substr(4) == "hello");
    PASS();
}

void test_encode_256_bytes() {
    TEST(encode_256_bytes);
    std::string payload(256, 'x');
    auto frame = framing::encode(payload);
    assert(frame.size() == 4 + 256);
    assert(static_cast<uint8_t>(frame[0]) == 0);
    assert(static_cast<uint8_t>(frame[1]) == 0);
    assert(static_cast<uint8_t>(frame[2]) == 1);
    assert(static_cast<uint8_t>(frame[3]) == 0);
    PASS();
}

void test_encode_large() {
    TEST(encode_large);
    std::string payload(70000, 'a');
    auto frame = framing::encode(payload);
    assert(frame.size() == 4 + 70000);
    uint32_t len = (static_cast<uint8_t>(frame[0]) << 24) |
                   (static_cast<uint8_t>(frame[1]) << 16) |
                   (static_cast<uint8_t>(frame[2]) << 8) |
                   static_cast<uint8_t>(frame[3]);
    assert(len == 70000);
    PASS();
}

void test_encode_json_rpc() {
    TEST(encode_json_rpc);
    std::string payload = R"({"jsonrpc":"2.0","id":1,"method":"ping","params":{}})";
    auto frame = framing::encode(payload);
    uint32_t expected_len = static_cast<uint32_t>(payload.size());
    uint32_t actual_len = (static_cast<uint8_t>(frame[0]) << 24) |
                          (static_cast<uint8_t>(frame[1]) << 16) |
                          (static_cast<uint8_t>(frame[2]) << 8) |
                          static_cast<uint8_t>(frame[3]);
    assert(actual_len == expected_len);
    assert(frame.substr(4) == payload);
    PASS();
}

void test_roundtrip_via_socketpair() {
    TEST(roundtrip_via_socketpair);
#ifdef _WIN32
    // Skip on Windows — socketpair not available
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(rc == 0);

    std::string original = R"({"jsonrpc":"2.0","id":42,"method":"test","params":{"key":"value"}})";

    // Write framed message
    bool write_ok = framing::write_message(fds[0], original);
    assert(write_ok);

    // Read framed message
    std::string received;
    bool read_ok = framing::read_message(fds[1], received);
    assert(read_ok);
    assert(received == original);

    close(fds[0]);
    close(fds[1]);
    PASS();
#endif
}

void test_multiple_messages_socketpair() {
    TEST(multiple_messages_socketpair);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(rc == 0);

    std::vector<std::string> messages = {
        R"({"jsonrpc":"2.0","id":1,"method":"ping"})",
        R"({"jsonrpc":"2.0","id":2,"method":"project.get_state","params":{}})",
        R"json({"jsonrpc":"2.0","id":3,"method":"lua.execute","params":{"code":"print('hello')"}})json"
    };

    // Write all
    for (const auto& msg : messages) {
        assert(framing::write_message(fds[0], msg));
    }

    // Read all
    for (const auto& expected : messages) {
        std::string received;
        assert(framing::read_message(fds[1], received));
        assert(received == expected);
    }

    close(fds[0]);
    close(fds[1]);
    PASS();
#endif
}

void test_read_closed_socket() {
    TEST(read_closed_socket);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Close writer immediately
    close(fds[0]);

    std::string out;
    bool ok = framing::read_message(fds[1], out);
    assert(!ok);

    close(fds[1]);
    PASS();
#endif
}

void test_write_closed_socket() {
    TEST(write_closed_socket);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Close reader
    close(fds[1]);

    // Ignore SIGPIPE so send returns error instead of killing process
    signal(SIGPIPE, SIG_IGN);

    bool ok = framing::write_message(fds[0], "hello");
    // May or may not fail on first write (OS buffer), but shouldn't crash
    (void)ok;

    close(fds[0]);
    PASS();
#endif
}

void test_read_truncated_header() {
    TEST(read_truncated_header);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Send only 2 bytes of a 4-byte header, then close
    char partial[] = {0x00, 0x00};
    send(fds[0], partial, 2, 0);
    close(fds[0]);

    std::string out;
    bool ok = framing::read_message(fds[1], out);
    assert(!ok);

    close(fds[1]);
    PASS();
#endif
}

void test_read_truncated_payload() {
    TEST(read_truncated_payload);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Header says 100 bytes, but only send 10
    char header[] = {0x00, 0x00, 0x00, 0x64}; // 100
    send(fds[0], header, 4, 0);
    send(fds[0], "short data", 10, 0);
    close(fds[0]);

    std::string out;
    bool ok = framing::read_message(fds[1], out);
    assert(!ok);

    close(fds[1]);
    PASS();
#endif
}

void test_read_zero_length_frame() {
    TEST(read_zero_length_frame);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Send a frame with length 0
    char header[] = {0x00, 0x00, 0x00, 0x00};
    send(fds[0], header, 4, 0);
    close(fds[0]);

    std::string out;
    bool ok = framing::read_message(fds[1], out);
    assert(!ok); // Zero-length rejected

    close(fds[1]);
    PASS();
#endif
}

void test_read_oversized_frame() {
    TEST(read_oversized_frame);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Send header claiming 11MB (over 10MB limit)
    uint32_t big = 11 * 1024 * 1024;
    char header[4];
    header[0] = (big >> 24) & 0xFF;
    header[1] = (big >> 16) & 0xFF;
    header[2] = (big >> 8) & 0xFF;
    header[3] = big & 0xFF;
    send(fds[0], header, 4, 0);
    close(fds[0]);

    std::string out;
    bool ok = framing::read_message(fds[1], out);
    assert(!ok); // Oversized rejected

    close(fds[1]);
    PASS();
#endif
}

void test_roundtrip_binary_payload() {
    TEST(roundtrip_binary_payload);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Payload with null bytes, newlines, high bytes
    std::string payload;
    payload += '\0';
    payload += '\n';
    payload += '\r';
    payload += '\xFF';
    payload += "normal text";
    payload += '\0';

    assert(framing::write_message(fds[0], payload));

    std::string received;
    assert(framing::read_message(fds[1], received));
    assert(received == payload);

    close(fds[0]);
    close(fds[1]);
    PASS();
#endif
}

void test_roundtrip_large_payload() {
    TEST(roundtrip_large_payload);
#ifdef _WIN32
    std::cout << "SKIP (Windows)" << std::endl;
    tests_passed++;
    return;
#else
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    // Use threads to avoid deadlock from kernel buffer limits
    std::string payload(256 * 1024, 'x'); // 256KB

    std::thread writer([&]() {
        assert(framing::write_message(fds[0], payload));
    });

    std::string received;
    assert(framing::read_message(fds[1], received));
    writer.join();

    assert(received.size() == payload.size());
    assert(received == payload);

    close(fds[0]);
    close(fds[1]);
    PASS();
#endif
}

void test_encode_decode_all_byte_lengths() {
    TEST(encode_decode_all_byte_lengths);
    // Test boundary lengths: 0xFF, 0x100, 0xFFFF, 0x10000
    std::vector<size_t> sizes = {1, 127, 128, 255, 256, 65535, 65536};
    for (size_t sz : sizes) {
        std::string payload(sz, 'a');
        auto frame = framing::encode(payload);
        uint32_t decoded_len = (static_cast<uint8_t>(frame[0]) << 24) |
                               (static_cast<uint8_t>(frame[1]) << 16) |
                               (static_cast<uint8_t>(frame[2]) << 8) |
                               static_cast<uint8_t>(frame[3]);
        assert(decoded_len == sz);
        assert(frame.size() == 4 + sz);
    }
    PASS();
}

int main() {
    std::cout << "Framing tests:" << std::endl;

    test_encode_empty();
    test_encode_small();
    test_encode_256_bytes();
    test_encode_large();
    test_encode_json_rpc();
    test_roundtrip_via_socketpair();
    test_multiple_messages_socketpair();
    test_read_closed_socket();
    test_write_closed_socket();
    test_read_truncated_header();
    test_read_truncated_payload();
    test_read_zero_length_frame();
    test_read_oversized_frame();
    test_roundtrip_binary_payload();
    test_roundtrip_large_payload();
    test_encode_decode_all_byte_lengths();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed." << std::endl;
    return (tests_passed == tests_run) ? 0 : 1;
}
