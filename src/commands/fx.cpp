#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

void register_fx(CommandRegistry& registry) {
    registry.register_command("fx.get_parameters", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_index"))
            throw std::runtime_error("Missing required parameters: track_index, fx_index");
        if (!api::GetTrack || !api::TrackFX_GetCount || !api::TrackFX_GetNumParams)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        int fx_idx = params["fx_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        int fx_count = api::TrackFX_GetCount(track);
        if (fx_idx < 0 || fx_idx >= fx_count)
            throw std::runtime_error("FX index out of range");

        char fx_name[256] = {0};
        if (api::TrackFX_GetFXName)
            api::TrackFX_GetFXName(track, fx_idx, fx_name, sizeof(fx_name));

        bool enabled = api::TrackFX_GetEnabled ?
            api::TrackFX_GetEnabled(track, fx_idx) : true;

        int num_params = api::TrackFX_GetNumParams(track, fx_idx);
        json parameters = json::array();

        for (int i = 0; i < num_params; i++) {
            double minval = 0, maxval = 1;
            double val = api::TrackFX_GetParam ?
                api::TrackFX_GetParam(track, fx_idx, i, &minval, &maxval) : 0.0;

            char param_name[256] = {0};
            if (api::TrackFX_GetParamName)
                api::TrackFX_GetParamName(track, fx_idx, i, param_name, sizeof(param_name));

            json p = {
                {"index", i},
                {"name", std::string(param_name)},
                {"value", val},
                {"min", minval},
                {"max", maxval}
            };

            char formatted[256] = {0};
            if (api::TrackFX_GetFormattedParamValue) {
                api::TrackFX_GetFormattedParamValue(track, fx_idx, i, formatted, sizeof(formatted));
                p["formatted"] = std::string(formatted);
            }

            parameters.push_back(p);
        }

        return {
            {"fx_name", std::string(fx_name)},
            {"enabled", enabled},
            {"param_count", num_params},
            {"parameters", parameters}
        };
    });

    registry.register_command("fx.add", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_name"))
            throw std::runtime_error("Missing required parameters: track_index, fx_name");
        if (!api::GetTrack || !api::TrackFX_AddByName)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        std::string fx_name = params["fx_name"].get<std::string>();
        int position = params.value("position", -1);

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Add FX");
        int fx_idx = api::TrackFX_AddByName(track, fx_name.c_str(), false, position);
        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", fx_idx >= 0},
            {"fx_index", fx_idx}
        };
    });

    registry.register_command("fx.remove", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_index"))
            throw std::runtime_error("Missing required parameters: track_index, fx_index");
        if (!api::GetTrack || !api::TrackFX_Delete)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        int fx_idx = params["fx_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Remove FX");
        bool ok = api::TrackFX_Delete(track, fx_idx);
        if (api::UpdateArrange) api::UpdateArrange();

        return {{"success", ok}};
    });

    registry.register_command("fx.set_parameter", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_index") ||
            !params.contains("param_index") || !params.contains("value"))
            throw std::runtime_error("Missing required parameters: track_index, fx_index, param_index, value");
        if (!api::GetTrack || !api::TrackFX_SetParam)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        int fx_idx = params["fx_index"].get<int>();
        int param_idx = params["param_index"].get<int>();
        double value = params["value"].get<double>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Set FX parameter");
        bool ok = api::TrackFX_SetParam(track, fx_idx, param_idx, value);

        return {{"success", ok}};
    });

    registry.register_command("fx.enable", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_index"))
            throw std::runtime_error("Missing required parameters: track_index, fx_index");
        if (!api::GetTrack || !api::TrackFX_SetEnabled)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        int fx_idx = params["fx_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Enable FX");
        api::TrackFX_SetEnabled(track, fx_idx, true);

        return {{"success", true}};
    });

    registry.register_command("fx.disable", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("fx_index"))
            throw std::runtime_error("Missing required parameters: track_index, fx_index");
        if (!api::GetTrack || !api::TrackFX_SetEnabled)
            throw std::runtime_error("FX API not available");

        int track_idx = params["track_index"].get<int>();
        int fx_idx = params["fx_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Disable FX");
        api::TrackFX_SetEnabled(track, fx_idx, false);

        return {{"success", true}};
    });
}

} // namespace commands
} // namespace reaserve
