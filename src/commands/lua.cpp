#include "command_registry.h"
#include "reaper_api.h"
#include "json_rpc.h"
#include <atomic>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace reaserve {
namespace commands {

static std::atomic<uint64_t> s_request_counter{0};

static std::string make_temp_lua_path() {
#ifdef _WIN32
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string path = std::string(tmp) + "reaserve_XXXXXX.lua";
    // Windows doesn't have mkstemp, use tmpnam pattern
    char tmpname[L_tmpnam];
    tmpnam(tmpname);
    return std::string(tmpname) + ".lua";
#else
    char tmpl[] = "/tmp/reaserve_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return "/tmp/reaserve_fallback.lua";
    ::close(fd);
    std::string path = std::string(tmpl) + ".lua";
    rename(tmpl, path.c_str());
    return path;
#endif
}

void register_lua(CommandRegistry& registry) {
    registry.register_command("lua.execute", [](const json& params) -> json {
        if (!params.contains("code") || !params["code"].is_string()) {
            throw std::runtime_error("Missing required parameter: code");
        }

        std::string code = params["code"].get<std::string>();
        std::string tmp_path = make_temp_lua_path();

        // Write Lua to temp file
        {
            std::ofstream out(tmp_path);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to create temp file");
            }
            out << code;
        }

        // Register as ReaScript action
        int cmd_id = api::AddRemoveReaScript(true, 0, tmp_path.c_str(), true);
        if (cmd_id == 0) {
            std::remove(tmp_path.c_str());
            throw std::runtime_error("Failed to register ReaScript");
        }

        // Execute on main thread (we're already in timer callback)
        api::Main_OnCommandEx(cmd_id, 0, nullptr);

        // Unregister and clean up
        api::AddRemoveReaScript(false, 0, tmp_path.c_str(), true);
        std::remove(tmp_path.c_str());

        return {{"success", true}};
    });

    registry.register_command("lua.execute_and_read", [](const json& params) -> json {
        if (!params.contains("code") || !params["code"].is_string()) {
            throw std::runtime_error("Missing required parameter: code");
        }

        std::string code = params["code"].get<std::string>();

        // Generate unique ExtState keys for this request
        uint64_t req_id = s_request_counter.fetch_add(1);
        std::string ext_key = "result_" + std::to_string(req_id);
        std::string err_key = "error_" + std::to_string(req_id);

        // Clear any stale entries
        if (api::HasExtState && api::HasExtState("reaserve", ext_key.c_str())) {
            api::DeleteExtState("reaserve", ext_key.c_str(), false);
        }
        if (api::HasExtState && api::HasExtState("reaserve", err_key.c_str())) {
            api::DeleteExtState("reaserve", err_key.c_str(), false);
        }

        // Build wrapped Lua code:
        // - Preamble: defines reaserve_output() + opens pcall
        // - User code
        // - Epilogue: closes pcall, reports errors via ExtState
        std::string preamble =
            "function reaserve_output(data)\n"
            "  reaper.SetExtState(\"reaserve\", \"" + ext_key + "\", data, false)\n"
            "end\n"
            "local __ok, __err = pcall(function()\n";

        std::string epilogue =
            "\nend)\n"
            "if not __ok then\n"
            "  reaper.SetExtState(\"reaserve\", \"" + err_key + "\", tostring(__err), false)\n"
            "end\n";

        std::string wrapped = preamble + code + epilogue;

        // Write to temp file
        std::string tmp_path = make_temp_lua_path();
        {
            std::ofstream out(tmp_path);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to create temp file");
            }
            out << wrapped;
        }

        // Register and execute
        int cmd_id = api::AddRemoveReaScript(true, 0, tmp_path.c_str(), true);
        if (cmd_id == 0) {
            std::remove(tmp_path.c_str());
            throw std::runtime_error("Failed to register ReaScript");
        }

        api::Main_OnCommandEx(cmd_id, 0, nullptr);
        api::AddRemoveReaScript(false, 0, tmp_path.c_str(), true);
        std::remove(tmp_path.c_str());

        // Check for Lua runtime error
        if (api::HasExtState && api::HasExtState("reaserve", err_key.c_str())) {
            std::string lua_error = api::GetExtState("reaserve", err_key.c_str());
            api::DeleteExtState("reaserve", err_key.c_str(), false);
            // Also clean up result key if somehow both were set
            if (api::HasExtState("reaserve", ext_key.c_str())) {
                api::DeleteExtState("reaserve", ext_key.c_str(), false);
            }
            throw std::runtime_error("Lua error: " + lua_error);
        }

        // Read result from ExtState
        if (!api::HasExtState || !api::HasExtState("reaserve", ext_key.c_str())) {
            throw std::runtime_error(
                "Lua script did not call reaserve_output()");
        }

        // Copy before delete — GetExtState returns pointer to internal storage
        std::string content = api::GetExtState("reaserve", ext_key.c_str());
        api::DeleteExtState("reaserve", ext_key.c_str(), false);

        try {
            return json::parse(content);
        } catch (const json::parse_error&) {
            return {{"raw", content}};
        }
    });
}

} // namespace commands
} // namespace reaserve
