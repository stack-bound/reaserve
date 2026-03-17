#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

void register_routing(CommandRegistry& registry) {
    registry.register_command("routing.list_sends", [](const json& params) -> json {
        if (!params.contains("track_index") || !params["track_index"].is_number_integer())
            throw std::runtime_error("Missing required parameter: track_index");
        if (!api::GetTrack || !api::GetTrackNumSends || !api::GetTrackSendInfo_Value)
            throw std::runtime_error("Routing API not available");

        int track_idx = params["track_index"].get<int>();
        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        // category 0 = sends, 1 = receives, -1 = hardware outputs
        int category = params.value("category", 0);
        int num_sends = api::GetTrackNumSends(track, category);

        json sends = json::array();
        for (int i = 0; i < num_sends; i++) {
            double volume = api::GetTrackSendInfo_Value(track, category, i, "D_VOL");
            double pan = api::GetTrackSendInfo_Value(track, category, i, "D_PAN");
            bool mute = (bool)api::GetTrackSendInfo_Value(track, category, i, "B_MUTE");
            int dest_chan = (int)api::GetTrackSendInfo_Value(track, category, i, "I_DSTCHAN");
            int src_chan = (int)api::GetTrackSendInfo_Value(track, category, i, "I_SRCCHAN");

            // Get destination track index
            json send_info = {
                {"index", i},
                {"volume", volume},
                {"pan", pan},
                {"mute", mute},
                {"dest_channel", dest_chan},
                {"src_channel", src_chan}
            };

            sends.push_back(send_info);
        }

        return {
            {"track_index", track_idx},
            {"category", category},
            {"count", num_sends},
            {"sends", sends}
        };
    });

    registry.register_command("routing.add_send", [](const json& params) -> json {
        if (!params.contains("src_track") || !params.contains("dest_track"))
            throw std::runtime_error("Missing required parameters: src_track, dest_track");
        if (!api::GetTrack || !api::CreateTrackSend)
            throw std::runtime_error("Routing API not available");

        int src_idx = params["src_track"].get<int>();
        int dest_idx = params["dest_track"].get<int>();

        MediaTrack* src = api::GetTrack(nullptr, src_idx);
        MediaTrack* dest = api::GetTrack(nullptr, dest_idx);
        if (!src) throw std::runtime_error("Source track not found");
        if (!dest) throw std::runtime_error("Destination track not found");

        UndoBlock undo("ReaServe: Add send");
        int send_idx = api::CreateTrackSend(src, dest);

        if (send_idx >= 0 && api::SetTrackSendInfo_Value) {
            if (params.contains("volume"))
                api::SetTrackSendInfo_Value(src, 0, send_idx, "D_VOL", params["volume"].get<double>());
            if (params.contains("pan"))
                api::SetTrackSendInfo_Value(src, 0, send_idx, "D_PAN", params["pan"].get<double>());
        }

        return {
            {"success", send_idx >= 0},
            {"send_index", send_idx}
        };
    });

    registry.register_command("routing.remove_send", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("send_index"))
            throw std::runtime_error("Missing required parameters: track_index, send_index");
        if (!api::GetTrack || !api::RemoveTrackSend)
            throw std::runtime_error("Routing API not available");

        int track_idx = params["track_index"].get<int>();
        int send_idx = params["send_index"].get<int>();
        int category = params.value("category", 0);

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        UndoBlock undo("ReaServe: Remove send");
        bool ok = api::RemoveTrackSend(track, category, send_idx);

        return {{"success", ok}};
    });
}

} // namespace commands
} // namespace reaserve
