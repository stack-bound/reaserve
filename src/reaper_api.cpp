#include "reaper_api.h"
#include <cstring>

namespace reaserve {
namespace api {

// Core
void (*ShowConsoleMsg)(const char* msg) = nullptr;
int (*plugin_register)(const char* name, void* infostruct) = nullptr;
const char* (*GetResourcePath)() = nullptr;
const char* (*GetAppVersion)() = nullptr;

// Lua/ReaScript
int (*AddRemoveReaScript)(bool add, int section, const char* fn, bool commit) = nullptr;
void (*Main_OnCommandEx)(int command, int flag, ReaProject* proj) = nullptr;

// Project
void (*GetProjectTimeSignature2)(ReaProject* proj, double* bpm, double* bpi) = nullptr;
double (*GetCursorPosition)() = nullptr;
int (*GetPlayState)() = nullptr;
void (*SetEditCurPos)(double time, bool moveview, bool seekplay) = nullptr;
double (*GetProjectLength)(ReaProject* proj) = nullptr;

// Tracks
int (*CountTracks)(ReaProject* proj) = nullptr;
MediaTrack* (*GetTrack)(ReaProject* proj, int trackidx) = nullptr;
double (*GetMediaTrackInfo_Value)(MediaTrack* tr, const char* parmname) = nullptr;
bool (*GetSetMediaTrackInfo_String)(MediaTrack* tr, const char* parmname, char* new_value, bool setNewValue) = nullptr;
bool (*SetMediaTrackInfo_Value)(MediaTrack* tr, const char* parmname, double newvalue) = nullptr;
void (*InsertTrackAtIndex)(int idx, bool wantDefaults) = nullptr;
bool (*DeleteTrack)(MediaTrack* tr) = nullptr;
MediaTrack* (*GetMasterTrack)(ReaProject* proj) = nullptr;

// Items
int (*CountMediaItems)(ReaProject* proj) = nullptr;
MediaItem* (*GetMediaItem)(ReaProject* proj, int itemidx) = nullptr;
int (*CountTrackMediaItems)(MediaTrack* tr) = nullptr;
MediaTrack* (*GetMediaItem_Track)(MediaItem* item) = nullptr;
MediaItem* (*GetTrackMediaItem)(MediaTrack* tr, int itemidx) = nullptr;
double (*GetMediaItemInfo_Value)(MediaItem* item, const char* parmname) = nullptr;
bool (*SetMediaItemInfo_Value)(MediaItem* item, const char* parmname, double newvalue) = nullptr;
int (*CountSelectedMediaItems)(ReaProject* proj) = nullptr;
MediaItem* (*GetSelectedMediaItem)(ReaProject* proj, int selitem) = nullptr;
MediaItem* (*SplitMediaItem)(MediaItem* item, double position) = nullptr;
bool (*DeleteTrackMediaItem)(MediaTrack* tr, MediaItem* item) = nullptr;
MediaItem* (*AddMediaItemToTrack)(MediaTrack* tr) = nullptr;

// Takes
MediaItem_Take* (*GetActiveTake)(MediaItem* item) = nullptr;
bool (*GetSetMediaItemTakeInfo_String)(MediaItem_Take* take, const char* parmname, char* new_value, bool setNewValue) = nullptr;
PCM_source* (*GetMediaItemTake_Source)(MediaItem_Take* take) = nullptr;
const char* (*GetMediaSourceType)(PCM_source* source, char* typebuf, int typebuf_sz) = nullptr;

// FX
int (*TrackFX_GetCount)(MediaTrack* tr) = nullptr;
bool (*TrackFX_GetFXName)(MediaTrack* tr, int fx, char* buf, int buf_sz) = nullptr;
int (*TrackFX_GetNumParams)(MediaTrack* tr, int fx) = nullptr;
double (*TrackFX_GetParam)(MediaTrack* tr, int fx, int param, double* minval, double* maxval) = nullptr;
bool (*TrackFX_GetParamName)(MediaTrack* tr, int fx, int param, char* buf, int buf_sz) = nullptr;
bool (*TrackFX_SetParam)(MediaTrack* tr, int fx, int param, double val) = nullptr;
int (*TrackFX_AddByName)(MediaTrack* tr, const char* fxname, bool recFX, int instantiate) = nullptr;
bool (*TrackFX_Delete)(MediaTrack* tr, int fx) = nullptr;
bool (*TrackFX_GetEnabled)(MediaTrack* tr, int fx) = nullptr;
void (*TrackFX_SetEnabled)(MediaTrack* tr, int fx, bool enabled) = nullptr;
bool (*TrackFX_GetFormattedParamValue)(MediaTrack* tr, int fx, int param, char* buf, int buf_sz) = nullptr;

// Transport
void (*OnPlayButton)() = nullptr;
void (*OnStopButton)() = nullptr;
void (*OnPauseButton)() = nullptr;
void (*CSurf_OnRecord)() = nullptr;

// MIDI
MediaItem* (*CreateNewMIDIItemInProj)(MediaTrack* tr, double starttime, double endtime, const bool* qnIn) = nullptr;
MediaItem_Take* (*GetMediaItemTake)(MediaItem* item, int tk) = nullptr;
void* (*MIDI_GetAllEvts)(MediaItem_Take* take, char* bufNeedBig, int* bufNeedBig_sz) = nullptr;
bool (*MIDI_SetAllEvts)(MediaItem_Take* take, const char* buf, int buf_sz) = nullptr;
int (*MIDI_CountEvts)(MediaItem_Take* take, int* notecnt, int* ccevtcnt, int* textsyxevtcnt) = nullptr;
bool (*MIDI_GetNote)(MediaItem_Take* take, int noteidx, bool* selected, bool* muted,
                     double* startppq, double* endppq, int* chan, int* pitch, int* vel) = nullptr;
bool (*MIDI_InsertNote)(MediaItem_Take* take, bool selected, bool muted,
                        double startppq, double endppq, int chan, int pitch, int vel,
                        const bool* noSort) = nullptr;
bool (*MIDI_DeleteNote)(MediaItem_Take* take, int noteidx) = nullptr;
void (*MIDI_Sort)(MediaItem_Take* take) = nullptr;
double (*MIDI_GetProjTimeFromPPQPos)(MediaItem_Take* take, double ppq) = nullptr;
double (*MIDI_GetPPQPosFromProjTime)(MediaItem_Take* take, double projtime) = nullptr;
double (*MIDI_GetProjQNFromPPQPos)(MediaItem_Take* take, double ppq) = nullptr;
double (*MIDI_GetPPQPosFromProjQN)(MediaItem_Take* take, double projqn) = nullptr;

// Markers
int (*CountProjectMarkers)(ReaProject* proj, int* num_markers, int* num_regions) = nullptr;
bool (*EnumProjectMarkers)(int idx, bool* isrgn, double* pos, double* rgnend, const char** name, int* markrgnindexnumber) = nullptr;
bool (*AddProjectMarker)(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx) = nullptr;
bool (*DeleteProjectMarker)(ReaProject* proj, int markrgnindexnumber, bool isrgn) = nullptr;

// Routing
int (*GetTrackNumSends)(MediaTrack* tr, int category) = nullptr;
double (*GetTrackSendInfo_Value)(MediaTrack* tr, int category, int sendidx, const char* parmname) = nullptr;
bool (*SetTrackSendInfo_Value)(MediaTrack* tr, int category, int sendidx, const char* parmname, double newvalue) = nullptr;
int (*CreateTrackSend)(MediaTrack* tr, MediaTrack* desttr) = nullptr;
bool (*RemoveTrackSend)(MediaTrack* tr, int category, int sendidx) = nullptr;

// Envelopes
TrackEnvelope* (*GetTrackEnvelopeByName)(MediaTrack* tr, const char* envname) = nullptr;
int (*CountEnvelopePoints)(TrackEnvelope* envelope) = nullptr;
bool (*GetEnvelopePoint)(TrackEnvelope* envelope, int ptidx, double* time, double* value,
                          int* shape, double* tension, bool* selected) = nullptr;
int (*InsertEnvelopePoint)(TrackEnvelope* envelope, double time, double value, int shape,
                            double tension, bool selected, bool* noSort) = nullptr;
bool (*DeleteEnvelopePointRange)(TrackEnvelope* envelope, double time_start, double time_end) = nullptr;
void (*Envelope_SortPoints)(TrackEnvelope* envelope) = nullptr;

// Undo
void (*Undo_BeginBlock)() = nullptr;
void (*Undo_EndBlock)(const char* descchange, int extraflags) = nullptr;
void (*UpdateArrange)() = nullptr;
void (*UpdateTimeline)() = nullptr;

// ExtState
void (*SetExtState)(const char* section, const char* key, const char* value, bool persist) = nullptr;
const char* (*GetExtState)(const char* section, const char* key) = nullptr;
bool (*HasExtState)(const char* section, const char* key) = nullptr;
void (*DeleteExtState)(const char* section, const char* key, bool persist) = nullptr;

#define RESOLVE(name) *(void**)&name = rec->GetFunc(#name)
#define REQUIRE(name) RESOLVE(name); if (!name) return false

bool resolve_api(reaper_plugin_info_t* rec) {
    if (!rec || !rec->GetFunc) return false;

    // Critical functions — plugin won't load without these
    REQUIRE(ShowConsoleMsg);
    REQUIRE(plugin_register);
    REQUIRE(GetResourcePath);
    REQUIRE(GetAppVersion);
    REQUIRE(AddRemoveReaScript);
    REQUIRE(Main_OnCommandEx);

    // Project info
    RESOLVE(GetProjectTimeSignature2);
    RESOLVE(GetCursorPosition);
    RESOLVE(GetPlayState);
    RESOLVE(SetEditCurPos);
    RESOLVE(GetProjectLength);

    // Tracks
    RESOLVE(CountTracks);
    RESOLVE(GetTrack);
    RESOLVE(GetMediaTrackInfo_Value);
    RESOLVE(GetSetMediaTrackInfo_String);
    RESOLVE(SetMediaTrackInfo_Value);
    RESOLVE(InsertTrackAtIndex);
    RESOLVE(DeleteTrack);
    RESOLVE(GetMasterTrack);

    // Items
    RESOLVE(CountMediaItems);
    RESOLVE(GetMediaItem);
    RESOLVE(CountTrackMediaItems);
    RESOLVE(GetMediaItem_Track);
    RESOLVE(GetTrackMediaItem);
    RESOLVE(GetMediaItemInfo_Value);
    RESOLVE(SetMediaItemInfo_Value);
    RESOLVE(CountSelectedMediaItems);
    RESOLVE(GetSelectedMediaItem);
    RESOLVE(SplitMediaItem);
    RESOLVE(DeleteTrackMediaItem);
    RESOLVE(AddMediaItemToTrack);

    // Takes
    RESOLVE(GetActiveTake);
    RESOLVE(GetSetMediaItemTakeInfo_String);
    RESOLVE(GetMediaItemTake_Source);
    RESOLVE(GetMediaSourceType);

    // FX
    RESOLVE(TrackFX_GetCount);
    RESOLVE(TrackFX_GetFXName);
    RESOLVE(TrackFX_GetNumParams);
    RESOLVE(TrackFX_GetParam);
    RESOLVE(TrackFX_GetParamName);
    RESOLVE(TrackFX_SetParam);
    RESOLVE(TrackFX_AddByName);
    RESOLVE(TrackFX_Delete);
    RESOLVE(TrackFX_GetEnabled);
    RESOLVE(TrackFX_SetEnabled);
    RESOLVE(TrackFX_GetFormattedParamValue);

    // Transport
    RESOLVE(OnPlayButton);
    RESOLVE(OnStopButton);
    RESOLVE(OnPauseButton);
    RESOLVE(CSurf_OnRecord);

    // MIDI
    RESOLVE(CreateNewMIDIItemInProj);
    RESOLVE(GetMediaItemTake);
    RESOLVE(MIDI_GetAllEvts);
    RESOLVE(MIDI_SetAllEvts);
    RESOLVE(MIDI_CountEvts);
    RESOLVE(MIDI_GetNote);
    RESOLVE(MIDI_InsertNote);
    RESOLVE(MIDI_DeleteNote);
    RESOLVE(MIDI_Sort);
    RESOLVE(MIDI_GetProjTimeFromPPQPos);
    RESOLVE(MIDI_GetPPQPosFromProjTime);
    RESOLVE(MIDI_GetProjQNFromPPQPos);
    RESOLVE(MIDI_GetPPQPosFromProjQN);

    // Markers
    RESOLVE(CountProjectMarkers);
    RESOLVE(EnumProjectMarkers);
    RESOLVE(AddProjectMarker);
    RESOLVE(DeleteProjectMarker);

    // Routing
    RESOLVE(GetTrackNumSends);
    RESOLVE(GetTrackSendInfo_Value);
    RESOLVE(SetTrackSendInfo_Value);
    RESOLVE(CreateTrackSend);
    RESOLVE(RemoveTrackSend);

    // Envelopes
    RESOLVE(GetTrackEnvelopeByName);
    RESOLVE(CountEnvelopePoints);
    RESOLVE(GetEnvelopePoint);
    RESOLVE(InsertEnvelopePoint);
    RESOLVE(DeleteEnvelopePointRange);
    RESOLVE(Envelope_SortPoints);

    // Undo
    RESOLVE(Undo_BeginBlock);
    RESOLVE(Undo_EndBlock);
    RESOLVE(UpdateArrange);
    RESOLVE(UpdateTimeline);

    // ExtState
    RESOLVE(SetExtState);
    RESOLVE(GetExtState);
    RESOLVE(HasExtState);
    RESOLVE(DeleteExtState);

    return true;
}

#undef RESOLVE
#undef REQUIRE

} // namespace api
} // namespace reaserve
