#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

void register_markers(CommandRegistry& registry) {
    registry.register_command("marker.list", [](const json& /*params*/) -> json {
        if (!api::CountProjectMarkers || !api::EnumProjectMarkers)
            throw std::runtime_error("Marker API not available");

        int num_markers = 0, num_regions = 0;
        int total = api::CountProjectMarkers(nullptr, &num_markers, &num_regions);

        json markers = json::array();
        json regions = json::array();

        for (int i = 0; i < total; i++) {
            bool isrgn = false;
            double pos = 0, rgnend = 0;
            const char* name = nullptr;
            int idx = 0;

            api::EnumProjectMarkers(i, &isrgn, &pos, &rgnend, &name, &idx);

            json entry = {
                {"index", idx},
                {"position", pos},
                {"name", name ? std::string(name) : ""}
            };

            if (isrgn) {
                entry["end_position"] = rgnend;
                regions.push_back(entry);
            } else {
                markers.push_back(entry);
            }
        }

        return {
            {"marker_count", num_markers},
            {"region_count", num_regions},
            {"markers", markers},
            {"regions", regions}
        };
    });

    registry.register_command("marker.add", [](const json& params) -> json {
        if (!params.contains("position") || !params["position"].is_number())
            throw std::runtime_error("Missing required parameter: position");
        if (!api::AddProjectMarker)
            throw std::runtime_error("Marker API not available");

        double position = params["position"].get<double>();
        std::string name = params.value("name", std::string(""));
        int wantidx = params.value("index", -1);
        bool isrgn = params.value("is_region", false);
        double rgnend = params.value("end_position", 0.0);

        UndoBlock undo("ReaServe: Add marker");
        bool ok = api::AddProjectMarker(nullptr, isrgn, position, rgnend,
                                         name.empty() ? nullptr : name.c_str(), wantidx);
        if (api::UpdateTimeline) api::UpdateTimeline();

        return {{"success", ok}};
    });

    registry.register_command("marker.remove", [](const json& params) -> json {
        if (!params.contains("index") || !params["index"].is_number_integer())
            throw std::runtime_error("Missing required parameter: index");
        if (!api::DeleteProjectMarker)
            throw std::runtime_error("Marker API not available");

        int index = params["index"].get<int>();
        bool isrgn = params.value("is_region", false);

        UndoBlock undo("ReaServe: Remove marker");
        bool ok = api::DeleteProjectMarker(nullptr, index, isrgn);
        if (api::UpdateTimeline) api::UpdateTimeline();

        return {{"success", ok}};
    });
}

} // namespace commands
} // namespace reaserve
