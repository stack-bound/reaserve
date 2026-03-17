#include "json_rpc.h"

namespace reaserve {

JsonRpcRequest parse_request(const std::string& raw) {
    JsonRpcRequest req;

    json j;
    try {
        j = json::parse(raw);
    } catch (const json::parse_error&) {
        req.error_code = ERR_PARSE;
        req.error_message = "Parse error";
        return req;
    }

    if (!j.is_object()) {
        req.error_code = ERR_INVALID_REQ;
        req.error_message = "Request must be a JSON object";
        return req;
    }

    // Check jsonrpc version
    if (!j.contains("jsonrpc") || j["jsonrpc"] != "2.0") {
        req.error_code = ERR_INVALID_REQ;
        req.error_message = "Missing or invalid jsonrpc version";
        return req;
    }

    // Check method
    if (!j.contains("method") || !j["method"].is_string()) {
        req.error_code = ERR_INVALID_REQ;
        req.error_message = "Missing or invalid method";
        return req;
    }

    req.method = j["method"].get<std::string>();

    // id is optional (notifications have no id), but we store it
    if (j.contains("id")) {
        req.id = j["id"];
    } else {
        req.id = nullptr;
    }

    // params: default to empty object
    if (j.contains("params")) {
        req.params = j["params"];
    } else {
        req.params = json::object();
    }

    req.valid = true;
    return req;
}

std::string make_result(const json& id, const json& result) {
    json resp = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    return resp.dump();
}

std::string make_error(const json& id, int code, const std::string& message) {
    json resp = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    return resp.dump();
}

std::string make_error(const json& id, int code, const std::string& message, const json& data) {
    json resp = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message},
            {"data", data}
        }}
    };
    return resp.dump();
}

} // namespace reaserve
