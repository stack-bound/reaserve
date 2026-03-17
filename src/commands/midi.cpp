#include "command_registry.h"
#include "reaper_api.h"
#include "undo.h"

namespace reaserve {
namespace commands {

void register_midi(CommandRegistry& registry) {
    registry.register_command("midi.get_notes", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("item_index"))
            throw std::runtime_error("Missing required parameters: track_index, item_index");
        if (!api::GetTrack || !api::GetTrackMediaItem || !api::GetActiveTake || !api::MIDI_CountEvts)
            throw std::runtime_error("MIDI API not available");

        int track_idx = params["track_index"].get<int>();
        int item_idx = params["item_index"].get<int>();

        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");
        MediaItem* item = api::GetTrackMediaItem(track, item_idx);
        if (!item) throw std::runtime_error("Item not found");
        MediaItem_Take* take = api::GetActiveTake(item);
        if (!take) throw std::runtime_error("No active take");

        int notecnt = 0, ccevtcnt = 0, textsyxevtcnt = 0;
        api::MIDI_CountEvts(take, &notecnt, &ccevtcnt, &textsyxevtcnt);

        json notes = json::array();
        for (int i = 0; i < notecnt; i++) {
            bool selected = false, muted = false;
            double startppq = 0, endppq = 0;
            int chan = 0, pitch = 0, vel = 0;

            if (api::MIDI_GetNote) {
                api::MIDI_GetNote(take, i, &selected, &muted, &startppq, &endppq, &chan, &pitch, &vel);
            }

            json note = {
                {"index", i},
                {"pitch", pitch},
                {"velocity", vel},
                {"channel", chan},
                {"start_ppq", startppq},
                {"end_ppq", endppq},
                {"selected", selected},
                {"muted", muted}
            };

            if (api::MIDI_GetProjTimeFromPPQPos) {
                note["start_time"] = api::MIDI_GetProjTimeFromPPQPos(take, startppq);
                note["end_time"] = api::MIDI_GetProjTimeFromPPQPos(take, endppq);
            }
            if (api::MIDI_GetProjQNFromPPQPos) {
                note["start_qn"] = api::MIDI_GetProjQNFromPPQPos(take, startppq);
                note["end_qn"] = api::MIDI_GetProjQNFromPPQPos(take, endppq);
            }

            notes.push_back(note);
        }

        return {
            {"note_count", notecnt},
            {"cc_count", ccevtcnt},
            {"notes", notes}
        };
    });

    registry.register_command("midi.insert_notes", [](const json& params) -> json {
        if (!params.contains("track_index") || !params.contains("notes"))
            throw std::runtime_error("Missing required parameters: track_index, notes");
        if (!api::GetTrack || !api::MIDI_InsertNote)
            throw std::runtime_error("MIDI API not available");

        int track_idx = params["track_index"].get<int>();
        MediaTrack* track = api::GetTrack(nullptr, track_idx);
        if (!track) throw std::runtime_error("Track not found");

        // If item_index provided, use existing item; otherwise create new MIDI item
        MediaItem* item = nullptr;
        MediaItem_Take* take = nullptr;

        UndoBlock undo("ReaServe: Insert MIDI notes");

        if (params.contains("item_index")) {
            int item_idx = params["item_index"].get<int>();
            if (!api::GetTrackMediaItem) throw std::runtime_error("Item API not available");
            item = api::GetTrackMediaItem(track, item_idx);
            if (!item) throw std::runtime_error("Item not found");
            if (!api::GetActiveTake) throw std::runtime_error("Take API not available");
            take = api::GetActiveTake(item);
            if (!take) throw std::runtime_error("No active take");
        } else {
            // Create a new MIDI item
            if (!api::CreateNewMIDIItemInProj) throw std::runtime_error("MIDI creation API not available");
            double start_time = params.value("start_time", 0.0);
            double end_time = params.value("end_time", start_time + 4.0);
            item = api::CreateNewMIDIItemInProj(track, start_time, end_time, nullptr);
            if (!item) throw std::runtime_error("Failed to create MIDI item");
            if (!api::GetActiveTake) throw std::runtime_error("Take API not available");
            take = api::GetActiveTake(item);
            if (!take) throw std::runtime_error("No take in new MIDI item");
        }

        const auto& notes = params["notes"];
        if (!notes.is_array()) throw std::runtime_error("notes must be an array");

        int inserted = 0;
        bool no_sort = true;

        for (const auto& note : notes) {
            int pitch = note.value("pitch", 60);
            int vel = note.value("velocity", 100);
            int chan = note.value("channel", 0);
            bool selected = note.value("selected", false);
            bool muted = note.value("muted", false);

            double startppq, endppq;

            if (note.contains("start_ppq") && note.contains("end_ppq")) {
                startppq = note["start_ppq"].get<double>();
                endppq = note["end_ppq"].get<double>();
            } else if (note.contains("start_qn") && note.contains("end_qn") && api::MIDI_GetPPQPosFromProjQN) {
                startppq = api::MIDI_GetPPQPosFromProjQN(take, note["start_qn"].get<double>());
                endppq = api::MIDI_GetPPQPosFromProjQN(take, note["end_qn"].get<double>());
            } else if (note.contains("start_time") && note.contains("end_time") && api::MIDI_GetPPQPosFromProjTime) {
                startppq = api::MIDI_GetPPQPosFromProjTime(take, note["start_time"].get<double>());
                endppq = api::MIDI_GetPPQPosFromProjTime(take, note["end_time"].get<double>());
            } else {
                continue; // Skip notes without position info
            }

            if (api::MIDI_InsertNote(take, selected, muted, startppq, endppq, chan, pitch, vel, &no_sort)) {
                inserted++;
            }
        }

        if (api::MIDI_Sort) api::MIDI_Sort(take);
        if (api::UpdateArrange) api::UpdateArrange();

        return {
            {"success", true},
            {"notes_inserted", inserted}
        };
    });
}

} // namespace commands
} // namespace reaserve
