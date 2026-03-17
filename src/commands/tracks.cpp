#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"
#include <cmath>

namespace reaserve {
namespace commands {

void register_tracks(CommandRegistry& registry) {
    registry.register_command("track.add", [](const json& params) -> json {
        if (!api::InsertTrackAtIndex || !api::GetTrack || !api::CountTracks)
            throw std::runtime_error("Track API not available");

        int num_tracks = api::CountTracks(nullptr);
        int index = params.value("index", num_tracks);
        std::string name = params.value("name", std::string(""));

        UndoBlock undo("ReaServe: Add track");
        api::InsertTrackAtIndex(index, true);

        if (!name.empty()) {
            MediaTrack* track = api::GetTrack(nullptr, index);
            if (track && api::GetSetMediaTrackInfo_String) {
                api::GetSetMediaTrackInfo_String(track, "P_NAME",
                    const_cast<char*>(name.c_str()), true);
            }
        }

        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", true},
            {"index", index},
            {"track_count", api::CountTracks(nullptr)}
        };
    });

    registry.register_command("track.remove", [](const json& params) -> json {
        if (!params.contains("index") || !params["index"].is_number_integer())
            throw std::runtime_error("Missing required parameter: index");
        if (!api::GetTrack || !api::DeleteTrack || !api::CountTracks)
            throw std::runtime_error("Track API not available");

        int index = params["index"].get<int>();
        MediaTrack* track = api::GetTrack(nullptr, index);
        if (!track) throw std::runtime_error("Track not found at index " + std::to_string(index));

        UndoBlock undo("ReaServe: Remove track");
        api::DeleteTrack(track);
        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", true},
            {"track_count", api::CountTracks(nullptr)}
        };
    });

    registry.register_command("track.set_property", [](const json& params) -> json {
        if (!params.contains("index") || !params["index"].is_number_integer())
            throw std::runtime_error("Missing required parameter: index");
        if (!params.contains("property") || !params["property"].is_string())
            throw std::runtime_error("Missing required parameter: property");
        if (!params.contains("value"))
            throw std::runtime_error("Missing required parameter: value");
        if (!api::GetTrack)
            throw std::runtime_error("Track API not available");

        int index = params["index"].get<int>();
        std::string prop = params["property"].get<std::string>();

        MediaTrack* track = api::GetTrack(nullptr, index);
        if (!track) throw std::runtime_error("Track not found at index " + std::to_string(index));

        UndoBlock undo("ReaServe: Set track property");

        if (prop == "name") {
            std::string val = params["value"].get<std::string>();
            if (api::GetSetMediaTrackInfo_String)
                api::GetSetMediaTrackInfo_String(track, "P_NAME",
                    const_cast<char*>(val.c_str()), true);
        } else if (prop == "volume_db") {
            double db = params["value"].get<double>();
            double vol = std::pow(10.0, db / 20.0);
            if (api::SetMediaTrackInfo_Value)
                api::SetMediaTrackInfo_Value(track, "D_VOL", vol);
        } else if (prop == "pan") {
            double pan = params["value"].get<double>();
            if (api::SetMediaTrackInfo_Value)
                api::SetMediaTrackInfo_Value(track, "D_PAN", pan);
        } else if (prop == "mute") {
            bool mute = params["value"].get<bool>();
            if (api::SetMediaTrackInfo_Value)
                api::SetMediaTrackInfo_Value(track, "B_MUTE", mute ? 1.0 : 0.0);
        } else if (prop == "solo") {
            bool solo = params["value"].get<bool>();
            if (api::SetMediaTrackInfo_Value)
                api::SetMediaTrackInfo_Value(track, "I_SOLO", solo ? 1.0 : 0.0);
        } else if (prop == "record_arm") {
            bool arm = params["value"].get<bool>();
            if (api::SetMediaTrackInfo_Value)
                api::SetMediaTrackInfo_Value(track, "I_RECARM", arm ? 1.0 : 0.0);
        } else {
            throw std::runtime_error("Unknown property: " + prop);
        }

        if (api::UpdateArrange) api::UpdateArrange();

        return {{"success", true}};
    });
}

} // namespace commands
} // namespace reaserve
