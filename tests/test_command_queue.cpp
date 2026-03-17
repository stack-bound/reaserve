#include "command_queue.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

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

void test_push_and_pop() {
    TEST(push_and_pop);
    CommandQueue q;
    auto future = q.push("ping", json::object(), 1);
    assert(q.size() == 1);

    auto cmd = q.try_pop();
    assert(cmd != nullptr);
    assert(cmd->method == "ping");
    assert(cmd->id == 1);
    assert(q.size() == 0);

    cmd->response.set_value("ok");
    assert(future.get() == "ok");
    PASS();
}

void test_try_pop_empty() {
    TEST(try_pop_empty);
    CommandQueue q;
    auto cmd = q.try_pop();
    assert(cmd == nullptr);
    PASS();
}

void test_fifo_order() {
    TEST(fifo_order);
    CommandQueue q;
    q.push("first", json::object(), 1);
    q.push("second", json::object(), 2);
    q.push("third", json::object(), 3);

    auto c1 = q.try_pop();
    auto c2 = q.try_pop();
    auto c3 = q.try_pop();

    assert(c1->method == "first");
    assert(c2->method == "second");
    assert(c3->method == "third");

    c1->response.set_value("");
    c2->response.set_value("");
    c3->response.set_value("");
    PASS();
}

void test_concurrent_push() {
    TEST(concurrent_push);
    CommandQueue q;
    constexpr int N = 100;
    std::vector<std::thread> threads;
    std::vector<std::future<std::string>> futures;
    std::mutex futures_mutex;

    for (int i = 0; i < N; i++) {
        threads.emplace_back([&q, &futures, &futures_mutex, i]() {
            auto f = q.push("method_" + std::to_string(i), json::object(), i);
            std::lock_guard<std::mutex> lock(futures_mutex);
            futures.push_back(std::move(f));
        });
    }

    for (auto& t : threads) t.join();
    assert(q.size() == N);

    // Drain and fulfill
    for (int i = 0; i < N; i++) {
        auto cmd = q.try_pop();
        assert(cmd != nullptr);
        cmd->response.set_value("done");
    }

    for (auto& f : futures) {
        assert(f.get() == "done");
    }
    PASS();
}

void test_future_blocks_until_fulfilled() {
    TEST(future_blocks_until_fulfilled);
    CommandQueue q;
    auto future = q.push("test", json::object(), 1);

    std::atomic<bool> fulfilled{false};
    std::thread consumer([&]() {
        auto cmd = q.try_pop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fulfilled = true;
        cmd->response.set_value("result");
    });

    std::string result = future.get();
    assert(fulfilled);
    assert(result == "result");
    consumer.join();
    PASS();
}

void test_concurrent_push_and_pop() {
    TEST(concurrent_push_and_pop);
    CommandQueue q;
    constexpr int N = 200;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> done_producing{false};

    // Producer threads — push N commands total
    std::vector<std::thread> producers;
    std::vector<std::future<std::string>> futures;
    std::mutex futures_mutex;

    for (int i = 0; i < N; i++) {
        producers.emplace_back([&q, &futures, &futures_mutex, &produced, i]() {
            auto f = q.push("m" + std::to_string(i), json::object(), i);
            std::lock_guard<std::mutex> lock(futures_mutex);
            futures.push_back(std::move(f));
            produced++;
        });
    }

    // Consumer thread — pop and fulfill, simulating timer callback
    std::thread consumer([&]() {
        while (!done_producing || q.size() > 0) {
            auto cmd = q.try_pop();
            if (cmd) {
                cmd->response.set_value("ok");
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producers) t.join();
    done_producing = true;
    consumer.join();

    assert(produced == N);
    assert(consumed == N);
    for (auto& f : futures) {
        assert(f.get() == "ok");
    }
    PASS();
}

void test_params_and_id_preserved() {
    TEST(params_and_id_preserved);
    CommandQueue q;

    json params1 = {{"name", "Bass"}, {"index", 2}};
    json params2 = {{"code", "print('hello')"}};

    auto f1 = q.push("track.add", params1, "req-1");
    auto f2 = q.push("lua.execute", params2, 42);

    auto c1 = q.try_pop();
    assert(c1->method == "track.add");
    assert(c1->params["name"] == "Bass");
    assert(c1->params["index"] == 2);
    assert(c1->id == "req-1");
    c1->response.set_value("r1");

    auto c2 = q.try_pop();
    assert(c2->method == "lua.execute");
    assert(c2->params["code"] == "print('hello')");
    assert(c2->id == 42);
    c2->response.set_value("r2");

    assert(f1.get() == "r1");
    assert(f2.get() == "r2");
    PASS();
}

void test_pop_interleaved_with_push() {
    TEST(pop_interleaved_with_push);
    CommandQueue q;

    // Push 1, pop 1, push 2, pop 2 — tests deque doesn't break with interleaving
    auto f1 = q.push("a", json::object(), 1);
    assert(q.size() == 1);
    auto c1 = q.try_pop();
    assert(c1->method == "a");
    assert(q.size() == 0);
    c1->response.set_value("a");

    auto f2 = q.push("b", json::object(), 2);
    auto f3 = q.push("c", json::object(), 3);
    assert(q.size() == 2);
    auto c2 = q.try_pop();
    assert(c2->method == "b");
    c2->response.set_value("b");

    auto f4 = q.push("d", json::object(), 4);
    assert(q.size() == 2);
    auto c3 = q.try_pop();
    auto c4 = q.try_pop();
    assert(c3->method == "c");
    assert(c4->method == "d");
    c3->response.set_value("c");
    c4->response.set_value("d");

    assert(f1.get() == "a");
    assert(f2.get() == "b");
    assert(f3.get() == "c");
    assert(f4.get() == "d");
    PASS();
}

void test_multiple_pops_on_empty() {
    TEST(multiple_pops_on_empty);
    CommandQueue q;
    for (int i = 0; i < 100; i++) {
        assert(q.try_pop() == nullptr);
    }
    assert(q.size() == 0);
    // Verify queue still works after many empty pops
    auto f = q.push("test", json::object(), 1);
    auto c = q.try_pop();
    assert(c != nullptr);
    assert(c->method == "test");
    c->response.set_value("ok");
    assert(f.get() == "ok");
    PASS();
}

int main() {
    std::cout << "Command queue tests:" << std::endl;

    test_push_and_pop();
    test_try_pop_empty();
    test_fifo_order();
    test_concurrent_push();
    test_future_blocks_until_fulfilled();
    test_concurrent_push_and_pop();
    test_params_and_id_preserved();
    test_pop_interleaved_with_push();
    test_multiple_pops_on_empty();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed." << std::endl;
    return (tests_passed == tests_run) ? 0 : 1;
}
