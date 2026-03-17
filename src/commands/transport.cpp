#include "command_registry.h"
#include "reaper_api.h"

namespace reaserve {
namespace commands {

void register_transport(CommandRegistry& registry) {
    registry.register_command("transport.get_state", [](const json& /*params*/) -> json {
        int state = api::GetPlayState ? api::GetPlayState() : 0;
        double cursor = api::GetCursorPosition ? api::GetCursorPosition() : 0.0;

        std::string state_name;
        if (state & 4) state_name = "recording";
        else if (state & 2) state_name = "paused";
        else if (state & 1) state_name = "playing";
        else state_name = "stopped";

        return {
            {"play_state", state},
            {"state_name", state_name},
            {"cursor_position", cursor},
            {"is_playing", (state & 1) != 0},
            {"is_paused", (state & 2) != 0},
            {"is_recording", (state & 4) != 0}
        };
    });

    registry.register_command("transport.control", [](const json& params) -> json {
        if (!params.contains("action") || !params["action"].is_string())
            throw std::runtime_error("Missing required parameter: action");

        std::string action = params["action"].get<std::string>();

        if (action == "play") {
            if (!api::OnPlayButton) throw std::runtime_error("Transport API not available");
            api::OnPlayButton();
        } else if (action == "stop") {
            if (!api::OnStopButton) throw std::runtime_error("Transport API not available");
            api::OnStopButton();
        } else if (action == "pause") {
            if (!api::OnPauseButton) throw std::runtime_error("Transport API not available");
            api::OnPauseButton();
        } else if (action == "record") {
            if (!api::CSurf_OnRecord) throw std::runtime_error("Transport API not available");
            api::CSurf_OnRecord();
        } else {
            throw std::runtime_error("Unknown action: " + action + " (valid: play, stop, pause, record)");
        }

        return {{"success", true}, {"action", action}};
    });

    registry.register_command("transport.set_cursor", [](const json& params) -> json {
        if (!params.contains("position") || !params["position"].is_number())
            throw std::runtime_error("Missing required parameter: position");
        if (!api::SetEditCurPos)
            throw std::runtime_error("Transport API not available");

        double position = params["position"].get<double>();
        bool moveview = params.value("moveview", true);
        bool seekplay = params.value("seekplay", true);

        api::SetEditCurPos(position, moveview, seekplay);

        return {
            {"success", true},
            {"position", position}
        };
    });
}

} // namespace commands
} // namespace reaserve
