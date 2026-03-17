#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

static json item_to_json(MediaItem* item, int index) {
    double position = api::GetMediaItemInfo_Value ?
        api::GetMediaItemInfo_Value(item, "D_POSITION") : 0.0;
    double length = api::GetMediaItemInfo_Value ?
        api::GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;
    bool mute = api::GetMediaItemInfo_Value ?
        (bool)api::GetMediaItemInfo_Value(item, "B_MUTE") : false;
    bool selected = api::GetMediaItemInfo_Value ?
        (bool)api::GetMediaItemInfo_Value(item, "B_UISEL") : false;

    json item_json = {
        {"index", index},
        {"position", position},
        {"length", length},
        {"mute", mute},
        {"selected", selected}
    };

    // Get take info
    if (api::GetActiveTake) {
        MediaItem_Take* take = api::GetActiveTake(item);
        if (take) {
            char name_buf[256] = {0};
            if (api::GetSetMediaItemTakeInfo_String)
                api::GetSetMediaItemTakeInfo_String(take, "P_NAME", name_buf, false);
            item_json["take_name"] = std::string(name_buf);

            if (api::GetMediaItemTake_Source && api::GetMediaSourceType) {
                PCM_source* src = api::GetMediaItemTake_Source(take);
                if (src) {
                    char type_buf[64] = {0};
                    api::GetMediaSourceType(src, type_buf, sizeof(type_buf));
                    item_json["source_type"] = std::string(type_buf);
                }
            }
        }
    }

    return item_json;
}

void register_items(CommandRegistry& registry) {
    registry.register_command("items.get_selected", [](const json& /*params*/) -> json {
        if (!api::CountSelectedMediaItems || !api::GetSelectedMediaItem)
            throw std::runtime_error("Item API not available");

        int count = api::CountSelectedMediaItems(nullptr);
        json items = json::array();

        for (int i = 0; i < count; i++) {
            MediaItem* item = api::GetSelectedMediaItem(nullptr, i);
            if (!item) continue;

            json item_json = item_to_json(item, i);

            // Find track index
            if (api::GetMediaItem_Track && api::CountTracks && api::GetTrack) {
                MediaTrack* track = api::GetMediaItem_Track(item);
                int num_tracks = api::CountTracks(nullptr);
                for (int t = 0; t < num_tracks; t++) {
                    if (api::GetTrack(nullptr, t) == track) {
                        item_json["track_index"] = t;
                        break;
                    }
                }
            }

            items.push_back(item_json);
        }

        return {
            {"count", count},
            {"items", items}
        };
    });

    registry.register_command("item.list", [](const json& params) -> json {
        if (!api::CountTracks || !api::GetTrack || !api::CountTrackMediaItems || !api::GetTrackMediaItem)
            throw std::runtime_error("Item API not available");

        json items = json::array();

        if (params.contains("track_index") && params["track_index"].is_number_integer()) {
            int track_idx = params["track_index"].get<int>();
            MediaTrack* track = api::GetTrack(nullptr, track_idx);
            if (!track) throw std::runtime_error("Track not found");

            int count = api::CountTrackMediaItems(track);
            for (int i = 0; i < count; i++) {
                MediaItem* item = api::GetTrackMediaItem(track, i);
                if (!item) continue;
                json j = item_to_json(item, i);
                j["track_index"] = track_idx;
                items.push_back(j);
            }
        } else {
            int num_tracks = api::CountTracks(nullptr);
            for (int t = 0; t < num_tracks; t++) {
                MediaTrack* track = api::GetTrack(nullptr, t);
                if (!track) continue;
                int count = api::CountTrackMediaItems(track);
                for (int i = 0; i < count; i++) {
                    MediaItem* item = api::GetTrackMediaItem(track, i);
                    if (!item) continue;
                    json j = item_to_json(item, i);
                    j["track_index"] = t;
                    items.push_back(j);
                }
            }
        }

        return {
            {"count", items.size()},
            {"items", items}
        };
    });

    registry.register_command("item.move", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("item_index") || !params.contains("position"))
            throw std::runtime_error("Missing required parameters: track_index, item_index, position");
        if (!api::GetTrack || !api::GetTrackMediaItem || !api::SetMediaItemInfo_Value)
            throw std::runtime_error("Item API not available");

        int track_idx = params["track_index"].get<int>();
        int item_idx = params["item_index"].get<int>();
        double position = params["position"].get<double>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");
        MediaItem* item = api::GetTrackMediaItem(track, item_idx);
        if (!item) throw std::runtime_error("Item not found");

        UndoBlock undo("ReaServe: Move item");
        api::SetMediaItemInfo_Value(item, "D_POSITION", position);
        if (api::UpdateArrange) api::UpdateArrange();

        return {{"success", true}};
    });

    registry.register_command("item.resize", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("item_index") || !params.contains("length"))
            throw std::runtime_error("Missing required parameters: track_index, item_index, length");
        if (!api::GetTrack || !api::GetTrackMediaItem || !api::SetMediaItemInfo_Value)
            throw std::runtime_error("Item API not available");

        int track_idx = params["track_index"].get<int>();
        int item_idx = params["item_index"].get<int>();
        double length = params["length"].get<double>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");
        MediaItem* item = api::GetTrackMediaItem(track, item_idx);
        if (!item) throw std::runtime_error("Item not found");

        UndoBlock undo("ReaServe: Resize item");
        api::SetMediaItemInfo_Value(item, "D_LENGTH", length);
        if (api::UpdateArrange) api::UpdateArrange();

        return {{"success", true}};
    });

    registry.register_command("item.split", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("item_index") || !params.contains("position"))
            throw std::runtime_error("Missing required parameters: track_index, item_index, position");
        if (!api::GetTrack || !api::GetTrackMediaItem || !api::SplitMediaItem)
            throw std::runtime_error("Item API not available");

        int track_idx = params["track_index"].get<int>();
        int item_idx = params["item_index"].get<int>();
        double position = params["position"].get<double>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");
        MediaItem* item = api::GetTrackMediaItem(track, item_idx);
        if (!item) throw std::runtime_error("Item not found");

        UndoBlock undo("ReaServe: Split item");
        MediaItem* new_item = api::SplitMediaItem(item, position);
        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", new_item != nullptr},
            {"split_position", position}
        };
    });

    registry.register_command("item.delete", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("item_index"))
            throw std::runtime_error("Missing required parameters: track_index, item_index");
        if (!api::GetTrack || !api::GetTrackMediaItem || !api::DeleteTrackMediaItem)
            throw std::runtime_error("Item API not available");

        int track_idx = params["track_index"].get<int>();
        int item_idx = params["item_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");
        MediaItem* item = api::GetTrackMediaItem(track, item_idx);
        if (!item) throw std::runtime_error("Item not found");

        UndoBlock undo("ReaServe: Delete item");
        bool ok = api::DeleteTrackMediaItem(track, item);
        if (api::UpdateArrange) api::UpdateArrange();

        return {{"success", ok}};
    });
}

} // namespace commands
} // namespace reaserve
