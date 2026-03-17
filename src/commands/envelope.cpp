#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

void register_envelope(CommandRegistry& registry) {
    registry.register_command("envelope.list", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("envelope_name"))
            throw std::runtime_error("Missing required parameters: track_index, envelope_name");
        if (!api::GetTrack || !api::GetTrackEnvelopeByName || !api::CountEnvelopePoints)
            throw std::runtime_error("Envelope API not available");

        int track_idx = params["track_index"].get<int>();
        std::string env_name = params["envelope_name"].get<std::string>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        TrackEnvelope* env = api::GetTrackEnvelopeByName(track, env_name.c_str());
        if (!env) throw std::runtime_error("Envelope not found: " + env_name);

        int count = api::CountEnvelopePoints(env);
        json points = json::array();

        for (int i = 0; i < count; i++) {
            double time = 0, value = 0, tension = 0;
            int shape = 0;
            bool selected = false;

            if (api::GetEnvelopePoint)
                api::GetEnvelopePoint(env, i, &time, &value, &shape, &tension, &selected);

            points.push_back({
                {"index", i},
                {"time", time},
                {"value", value},
                {"shape", shape},
                {"tension", tension},
                {"selected", selected}
            });
        }

        return {
            {"envelope_name", env_name},
            {"point_count", count},
            {"points", points}
        };
    });

    registry.register_command("envelope.add_point", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("envelope_name") ||
            !params.contains("time") || !params.contains("value"))
            throw std::runtime_error("Missing required parameters: track_index, envelope_name, time, value");
        if (!api::GetTrack || !api::GetTrackEnvelopeByName || !api::InsertEnvelopePoint)
            throw std::runtime_error("Envelope API not available");

        int track_idx = params["track_index"].get<int>();
        std::string env_name = params["envelope_name"].get<std::string>();
        double time = params["time"].get<double>();
        double value = params["value"].get<double>();
        int shape = params.value("shape", 0);
        double tension = params.value("tension", 0.0);
        bool selected = params.value("selected", false);

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        TrackEnvelope* env = api::GetTrackEnvelopeByName(track, env_name.c_str());
        if (!env) throw std::runtime_error("Envelope not found: " + env_name);

        UndoBlock undo("ReaServe: Add envelope point");
        bool no_sort = true;
        int idx = api::InsertEnvelopePoint(env, time, value, shape, tension, selected, &no_sort);

        if (api::Envelope_SortPoints) api::Envelope_SortPoints(env);
        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", idx >= 0},
            {"point_index", idx}
        };
    });
}

} // namespace commands
} // namespace reaserve
