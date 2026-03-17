#include "command_registry.h"
#include "reaper_api.h"
#include <cmath>

namespace reaserve {
namespace commands {

void register_project(CommandRegistry& registry) {
    registry.register_command("project.get_state", [](const json& /*params*/) -> json {
        double bpm = 120.0, bpi = 4.0;
        if (api::GetProjectTimeSignature2)
            api::GetProjectTimeSignature2(nullptr, &bpm, &bpi);

        int num_tracks = api::CountTracks ? api::CountTracks(nullptr) : 0;
        double cursor = api::GetCursorPosition ? api::GetCursorPosition() : 0.0;
        int play_state = api::GetPlayState ? api::GetPlayState() : 0;
        double project_length = api::GetProjectLength ? api::GetProjectLength(nullptr) : 0.0;

        json tracks = json::array();
        for (int i = 0; i < num_tracks; i++) {
            MediaTrack* track = api::GetTrack(nullptr, i);
            if (!track) continue;

            char name_buf[256] = {0};
            if (api::GetSetMediaTrackInfo_String)
                api::GetSetMediaTrackInfo_String(track, "P_NAME", name_buf, false);

            double vol = api::GetMediaTrackInfo_Value ?
                api::GetMediaTrackInfo_Value(track, "D_VOL") : 1.0;
            double vol_db = (vol > 0) ? 20.0 * std::log10(vol) : -150.0;

            double pan = api::GetMediaTrackInfo_Value ?
                api::GetMediaTrackInfo_Value(track, "D_PAN") : 0.0;
            bool mute = api::GetMediaTrackInfo_Value ?
                (bool)api::GetMediaTrackInfo_Value(track, "B_MUTE") : false;
            int solo = api::GetMediaTrackInfo_Value ?
                (int)api::GetMediaTrackInfo_Value(track, "I_SOLO") : 0;
            int recarm = api::GetMediaTrackInfo_Value ?
                (int)api::GetMediaTrackInfo_Value(track, "I_RECARM") : 0;
            int fx_count = api::TrackFX_GetCount ?
                api::TrackFX_GetCount(track) : 0;
            int item_count = api::CountTrackMediaItems ?
                api::CountTrackMediaItems(track) : 0;

            tracks.push_back({
                {"index", i},
                {"name", std::string(name_buf)},
                {"volume_db", std::round(vol_db * 10.0) / 10.0},
                {"pan", pan},
                {"mute", mute},
                {"solo", solo != 0},
                {"record_arm", recarm != 0},
                {"fx_count", fx_count},
                {"item_count", item_count}
            });
        }

        return {
            {"bpm", bpm},
            {"time_sig_num", (int)bpi},
            {"time_sig_denom", 4},
            {"track_count", num_tracks},
            {"cursor_position", cursor},
            {"play_state", play_state},
            {"project_length", project_length},
            {"tracks", tracks}
        };
    });
}

} // namespace commands
} // namespace reaserve
