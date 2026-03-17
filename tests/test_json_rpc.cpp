#include "json_rpc.h"
#include <cassert>
#include <iostream>

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

void test_parse_valid_request() {
    TEST(parse_valid_request);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"ping","params":{}})");
    assert(req.valid);
    assert(req.method == "ping");
    assert(req.id == 1);
    assert(req.params.is_object());
    PASS();
}

void test_parse_string_id() {
    TEST(parse_string_id);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":"abc","method":"test","params":{}})");
    assert(req.valid);
    assert(req.id == "abc");
    PASS();
}

void test_parse_no_params() {
    TEST(parse_no_params);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"test"})");
    assert(req.valid);
    assert(req.params.is_object());
    PASS();
}

void test_parse_notification() {
    TEST(parse_notification);
    auto req = parse_request(R"({"jsonrpc":"2.0","method":"notify"})");
    assert(req.valid);
    assert(req.id.is_null());
    PASS();
}

void test_parse_invalid_json() {
    TEST(parse_invalid_json);
    auto req = parse_request("not json at all");
    assert(!req.valid);
    assert(req.error_code == ERR_PARSE);
    PASS();
}

void test_parse_missing_jsonrpc() {
    TEST(parse_missing_jsonrpc);
    auto req = parse_request(R"({"id":1,"method":"test"})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_wrong_version() {
    TEST(parse_wrong_version);
    auto req = parse_request(R"({"jsonrpc":"1.0","id":1,"method":"test"})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_missing_method() {
    TEST(parse_missing_method);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_non_object() {
    TEST(parse_non_object);
    auto req = parse_request("[1,2,3]");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_make_result() {
    TEST(make_result);
    auto resp = make_result(1, {{"pong", true}});
    auto j = json::parse(resp);
    assert(j["jsonrpc"] == "2.0");
    assert(j["id"] == 1);
    assert(j["result"]["pong"] == true);
    assert(!j.contains("error"));
    PASS();
}

void test_make_error() {
    TEST(make_error);
    auto resp = make_error(1, ERR_NOT_FOUND, "Method not found");
    auto j = json::parse(resp);
    assert(j["jsonrpc"] == "2.0");
    assert(j["id"] == 1);
    assert(j["error"]["code"] == ERR_NOT_FOUND);
    assert(j["error"]["message"] == "Method not found");
    assert(!j.contains("result"));
    PASS();
}

void test_make_error_with_data() {
    TEST(make_error_with_data);
    auto resp = make_error(1, ERR_INTERNAL, "Error", {{"detail", "something"}});
    auto j = json::parse(resp);
    assert(j["error"]["data"]["detail"] == "something");
    PASS();
}

void test_parse_with_params() {
    TEST(parse_with_params);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":42,"method":"track.add","params":{"name":"Bass","index":2}})");
    assert(req.valid);
    assert(req.method == "track.add");
    assert(req.params["name"] == "Bass");
    assert(req.params["index"] == 2);
    PASS();
}

void test_parse_method_non_string() {
    TEST(parse_method_non_string);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":42})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_empty_string() {
    TEST(parse_empty_string);
    auto req = parse_request("");
    assert(!req.valid);
    assert(req.error_code == ERR_PARSE);
    PASS();
}

void test_parse_null_id() {
    TEST(parse_null_id);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":null,"method":"test"})");
    assert(req.valid);
    assert(req.id.is_null());
    PASS();
}

void test_parse_array_params() {
    TEST(parse_array_params);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"test","params":[1,2,3]})");
    assert(req.valid);
    assert(req.params.is_array());
    assert(req.params.size() == 3);
    PASS();
}

void test_parse_nested_params() {
    TEST(parse_nested_params);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"test","params":{"a":{"b":{"c":true}}}})");
    assert(req.valid);
    assert(req.params["a"]["b"]["c"] == true);
    PASS();
}

void test_parse_unicode_method() {
    TEST(parse_unicode_method);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"test.fünction","params":{}})");
    assert(req.valid);
    assert(req.method == "test.fünction");
    PASS();
}

void test_parse_large_id() {
    TEST(parse_large_id);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":999999999,"method":"test"})");
    assert(req.valid);
    assert(req.id == 999999999);
    PASS();
}

void test_parse_method_is_bool() {
    TEST(parse_method_is_bool);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":true})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_method_is_null() {
    TEST(parse_method_is_null);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":null})");
    assert(!req.valid);
    assert(req.error_code == ERR_INVALID_REQ);
    PASS();
}

void test_parse_extra_fields_ignored() {
    TEST(parse_extra_fields_ignored);
    auto req = parse_request(R"({"jsonrpc":"2.0","id":1,"method":"test","extra":"stuff","more":123})");
    assert(req.valid);
    assert(req.method == "test");
    PASS();
}

void test_make_result_null_id() {
    TEST(make_result_null_id);
    auto resp = make_result(nullptr, {{"ok", true}});
    auto j = json::parse(resp);
    assert(j["id"].is_null());
    assert(j["result"]["ok"] == true);
    PASS();
}

void test_make_result_string_id() {
    TEST(make_result_string_id);
    auto resp = make_result("req-42", {{"value", 3.14}});
    auto j = json::parse(resp);
    assert(j["id"] == "req-42");
    assert(j["result"]["value"] == 3.14);
    PASS();
}

void test_make_error_null_id() {
    TEST(make_error_null_id);
    auto resp = make_error(nullptr, ERR_PARSE, "Parse error");
    auto j = json::parse(resp);
    assert(j["id"].is_null());
    assert(j["error"]["code"] == ERR_PARSE);
    PASS();
}

void test_make_result_complex_payload() {
    TEST(make_result_complex_payload);
    json result = {
        {"tracks", json::array({
            {{"name", "Drums"}, {"volume_db", -3.5}},
            {{"name", "Bass"}, {"volume_db", 0.0}}
        })},
        {"bpm", 120},
        {"empty_array", json::array()}
    };
    auto resp = make_result(1, result);
    auto j = json::parse(resp);
    assert(j["result"]["tracks"].size() == 2);
    assert(j["result"]["tracks"][0]["name"] == "Drums");
    assert(j["result"]["empty_array"].is_array());
    assert(j["result"]["empty_array"].empty());
    PASS();
}

int main() {
    std::cout << "JSON-RPC tests:" << std::endl;

    test_parse_valid_request();
    test_parse_string_id();
    test_parse_no_params();
    test_parse_notification();
    test_parse_invalid_json();
    test_parse_missing_jsonrpc();
    test_parse_wrong_version();
    test_parse_missing_method();
    test_parse_non_object();
    test_make_result();
    test_make_error();
    test_make_error_with_data();
    test_parse_with_params();
    test_parse_method_non_string();
    test_parse_empty_string();
    test_parse_null_id();
    test_parse_array_params();
    test_parse_nested_params();
    test_parse_unicode_method();
    test_parse_large_id();
    test_parse_method_is_bool();
    test_parse_method_is_null();
    test_parse_extra_fields_ignored();
    test_make_result_null_id();
    test_make_result_string_id();
    test_make_error_null_id();
    test_make_result_complex_payload();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed." << std::endl;
    return (tests_passed == tests_run) ? 0 : 1;
}
