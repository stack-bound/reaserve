#include "reaper_plugin.h"
#include "reaper_api.h"
#include "tcp_server.h"
#include "json_rpc.h"
#include "command_queue.h"
#include "command_registry.h"
#include "config.h"
#include "version.h"
#include <memory>
#include <string>
#include <chrono>
#include <future>

// Forward declarations for command registration
namespace reaserve { namespace commands {
    void register_ping(CommandRegistry& registry);
    void register_lua(CommandRegistry& registry);
    void register_project(CommandRegistry& registry);
    void register_tracks(CommandRegistry& registry);
    void register_items(CommandRegistry& registry);
    void register_fx(CommandRegistry& registry);
    void register_transport(CommandRegistry& registry);
    void register_midi(CommandRegistry& registry);
    void register_markers(CommandRegistry& registry);
    void register_routing(CommandRegistry& registry);
    void register_envelope(CommandRegistry& registry);
}}

static reaserve::CommandQueue g_queue;
static reaserve::CommandRegistry g_registry;
static std::unique_ptr<reaserve::TcpServer> g_server;

static void timer_callback() {
    for (int i = 0; i < 5; i++) {
        auto cmd = g_queue.try_pop();
        if (!cmd) break;

        std::string response;
        try {
            if (!g_registry.has_command(cmd->method)) {
                response = reaserve::make_error(cmd->id, reaserve::ERR_NOT_FOUND,
                    "Method not found: " + cmd->method);
            } else {
                auto result = g_registry.dispatch(cmd->method, cmd->params);
                response = reaserve::make_result(cmd->id, result);
            }
        } catch (const std::exception& e) {
            response = reaserve::make_error(cmd->id, reaserve::ERR_REAPER_API, e.what());
        }

        cmd->response.set_value(std::move(response));
    }
}

static std::string handle_request(const std::string& raw) {
    auto req = reaserve::parse_request(raw);

    if (!req.valid) {
        return reaserve::make_error(req.id, req.error_code, req.error_message);
    }

    // Push to queue and wait for main thread to process
    auto future = g_queue.push(req.method, req.params, req.id);

    // Wait with timeout
    auto status = future.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::timeout) {
        return reaserve::make_error(req.id, reaserve::ERR_TIMEOUT,
            "Main thread did not process command within 10 seconds");
    }

    return future.get();
}

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
    REAPER_PLUGIN_HINSTANCE hInstance,
    reaper_plugin_info_t* rec)
{
    (void)hInstance;

    if (!rec) {
        // Unloading
        if (g_server) {
            g_server->stop();
            g_server.reset();
        }
        return 0;
    }

    if (rec->caller_version != REAPER_PLUGIN_VERSION) {
        return 0;
    }

    // Resolve REAPER API function pointers
    if (!reaserve::api::resolve_api(rec)) {
        return 0;
    }

    // Register all commands
    reaserve::commands::register_ping(g_registry);
    reaserve::commands::register_lua(g_registry);
    reaserve::commands::register_project(g_registry);
    reaserve::commands::register_tracks(g_registry);
    reaserve::commands::register_items(g_registry);
    reaserve::commands::register_fx(g_registry);
    reaserve::commands::register_transport(g_registry);
    reaserve::commands::register_midi(g_registry);
    reaserve::commands::register_markers(g_registry);
    reaserve::commands::register_routing(g_registry);
    reaserve::commands::register_envelope(g_registry);

    // Register timer callback
    reaserve::api::plugin_register("timer", (void*)timer_callback);

    // Load configuration
    auto config = reaserve::load_config(reaserve::api::GetResourcePath());

    // Start TCP server
    g_server = std::make_unique<reaserve::TcpServer>(config.bind, config.port, handle_request);
    if (g_server->start()) {
        std::string msg = "ReaServe v" REASERVE_VERSION ": TCP server started on port " + std::to_string(config.port) + "\n";
        reaserve::api::ShowConsoleMsg(msg.c_str());
    } else {
        std::string msg = "ReaServe v" REASERVE_VERSION ": Failed to start TCP server on port " + std::to_string(config.port) + "\n";
        reaserve::api::ShowConsoleMsg(msg.c_str());
    }

    return 1; // Success
}

} // extern "C"
