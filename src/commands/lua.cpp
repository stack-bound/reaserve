#include "command_registry.h"
#include "reaper_api.h"
#include "json_rpc.h"
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
    char tmpl[] = "/tmp/reaserve_XXXXXX.lua";
    // mkstemp needs template without .lua suffix, so we use a different approach
    char tmpl2[] = "/tmp/reaserve_XXXXXX";
    int fd = mkstemp(tmpl2);
    if (fd < 0) return "/tmp/reaserve_fallback.lua";
    ::close(fd);
    std::string path = std::string(tmpl2) + ".lua";
    rename(tmpl2, path.c_str());
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
        if (!params.contains("state_path") || !params["state_path"].is_string()) {
            throw std::runtime_error("Missing required parameter: state_path");
        }

        std::string code = params["code"].get<std::string>();
        std::string state_path = params["state_path"].get<std::string>();

        // Delete stale state file
        std::remove(state_path.c_str());

        // Write Lua to temp file
        std::string tmp_path = make_temp_lua_path();
        {
            std::ofstream out(tmp_path);
            if (!out.is_open()) {
                throw std::runtime_error("Failed to create temp file");
            }
            out << code;
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

        // Read the state file
        std::ifstream in(state_path);
        if (!in.is_open()) {
            throw std::runtime_error("Lua script did not produce state file");
        }

        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        std::remove(state_path.c_str());

        try {
            return json::parse(content);
        } catch (const json::parse_error&) {
            return {{"raw", content}};
        }
    });
}

} // namespace commands
} // namespace reaserve
