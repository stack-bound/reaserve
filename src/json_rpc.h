#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace reaserve {

using json = nlohmann::json;

struct JsonRpcRequest {
    std::string method;
    json params;
    json id; // string, int, or null
    bool valid = false;
    std::string error_message;
    int error_code = 0;
};

// Error codes per JSON-RPC 2.0 spec + custom
constexpr int ERR_PARSE       = -32700;
constexpr int ERR_INVALID_REQ = -32600;
constexpr int ERR_NOT_FOUND   = -32601;
constexpr int ERR_INVALID_PARAMS = -32602;
constexpr int ERR_INTERNAL    = -32603;
constexpr int ERR_REAPER_API  = -32000;
constexpr int ERR_LUA         = -32001;
constexpr int ERR_TIMEOUT     = -32002;

// Parse a JSON-RPC 2.0 request from raw JSON string
JsonRpcRequest parse_request(const std::string& raw);

// Serialize a successful result
std::string make_result(const json& id, const json& result);

// Serialize an error response
std::string make_error(const json& id, int code, const std::string& message);

// Serialize an error response with data
std::string make_error(const json& id, int code, const std::string& message, const json& data);

} // namespace reaserve
