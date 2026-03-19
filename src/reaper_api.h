#pragma once

#include "reaper_plugin.h"
#include <cstdio>

// Forward declarations for REAPER types
// These are opaque pointers — we never dereference them directly
struct MediaTrack;
struct MediaItem;
struct MediaItem_Take;
struct TrackEnvelope;
struct ReaProject;
struct PCM_source;
struct MIDI_eventlist;

namespace reaserve {
namespace api {

// Core
extern void (*ShowConsoleMsg)(const char* msg);
extern int (*plugin_register)(const char* name, void* infostruct);
extern const char* (*GetResourcePath)();
extern const char* (*GetAppVersion)();

// Lua/ReaScript execution
extern int (*AddRemoveReaScript)(bool add, int section, const char* fn, bool commit);
extern void (*Main_OnCommandEx)(int command, int flag, ReaProject* proj);

// Project info
extern void (*GetProjectTimeSignature2)(ReaProject* proj, double* bpm, double* bpi);
extern double (*GetCursorPosition)();
extern int (*GetPlayState)();
extern void (*SetEditCurPos)(double time, bool moveview, bool seekplay);
extern double (*GetProjectLength)(ReaProject* proj);

// Tracks
extern int (*CountTracks)(ReaProject* proj);
extern MediaTrack* (*GetTrack)(ReaProject* proj, int trackidx);
extern double (*GetMediaTrackInfo_Value)(MediaTrack* tr, const char* parmname);
extern bool (*GetSetMediaTrackInfo_String)(MediaTrack* tr, const char* parmname, char* new_value, bool setNewValue);
extern bool (*SetMediaTrackInfo_Value)(MediaTrack* tr, const char* parmname, double newvalue);
extern void (*InsertTrackAtIndex)(int idx, bool wantDefaults);
extern bool (*DeleteTrack)(MediaTrack* tr);
extern MediaTrack* (*GetMasterTrack)(ReaProject* proj);

// Items
extern int (*CountMediaItems)(ReaProject* proj);
extern MediaItem* (*GetMediaItem)(ReaProject* proj, int itemidx);
extern int (*CountTrackMediaItems)(MediaTrack* tr);
extern MediaTrack* (*GetMediaItem_Track)(MediaItem* item);
extern MediaItem* (*GetTrackMediaItem)(MediaTrack* tr, int itemidx);
extern double (*GetMediaItemInfo_Value)(MediaItem* item, const char* parmname);
extern bool (*SetMediaItemInfo_Value)(MediaItem* item, const char* parmname, double newvalue);
extern int (*CountSelectedMediaItems)(ReaProject* proj);
extern MediaItem* (*GetSelectedMediaItem)(ReaProject* proj, int selitem);
extern MediaItem* (*SplitMediaItem)(MediaItem* item, double position);
extern bool (*DeleteTrackMediaItem)(MediaTrack* tr, MediaItem* item);
extern MediaItem* (*AddMediaItemToTrack)(MediaTrack* tr);

// Takes
extern MediaItem_Take* (*GetActiveTake)(MediaItem* item);
extern bool (*GetSetMediaItemTakeInfo_String)(MediaItem_Take* take, const char* parmname, char* new_value, bool setNewValue);
extern PCM_source* (*GetMediaItemTake_Source)(MediaItem_Take* take);
extern const char* (*GetMediaSourceType)(PCM_source* source, char* typebuf, int typebuf_sz);

// FX
extern int (*TrackFX_GetCount)(MediaTrack* tr);
extern bool (*TrackFX_GetFXName)(MediaTrack* tr, int fx, char* buf, int buf_sz);
extern int (*TrackFX_GetNumParams)(MediaTrack* tr, int fx);
extern double (*TrackFX_GetParam)(MediaTrack* tr, int fx, int param, double* minval, double* maxval);
extern bool (*TrackFX_GetParamName)(MediaTrack* tr, int fx, int param, char* buf, int buf_sz);
extern bool (*TrackFX_SetParam)(MediaTrack* tr, int fx, int param, double val);
extern int (*TrackFX_AddByName)(MediaTrack* tr, const char* fxname, bool recFX, int instantiate);
extern bool (*TrackFX_Delete)(MediaTrack* tr, int fx);
extern bool (*TrackFX_GetEnabled)(MediaTrack* tr, int fx);
extern void (*TrackFX_SetEnabled)(MediaTrack* tr, int fx, bool enabled);
extern bool (*TrackFX_GetFormattedParamValue)(MediaTrack* tr, int fx, int param, char* buf, int buf_sz);

// Transport
extern void (*OnPlayButton)();
extern void (*OnStopButton)();
extern void (*OnPauseButton)();
extern void (*CSurf_OnRecord)();

// MIDI
extern MediaItem* (*CreateNewMIDIItemInProj)(MediaTrack* tr, double starttime, double endtime, const bool* qnIn);
extern MediaItem_Take* (*GetMediaItemTake)(MediaItem* item, int tk);
extern void* (*MIDI_GetAllEvts)(MediaItem_Take* take, char* bufNeedBig, int* bufNeedBig_sz);
extern bool (*MIDI_SetAllEvts)(MediaItem_Take* take, const char* buf, int buf_sz);
extern int (*MIDI_CountEvts)(MediaItem_Take* take, int* notecnt, int* ccevtcnt, int* textsyxevtcnt);
extern bool (*MIDI_GetNote)(MediaItem_Take* take, int noteidx, bool* selected, bool* muted,
                            double* startppq, double* endppq, int* chan, int* pitch, int* vel);
extern bool (*MIDI_InsertNote)(MediaItem_Take* take, bool selected, bool muted,
                               double startppq, double endppq, int chan, int pitch, int vel,
                               const bool* noSort);
extern bool (*MIDI_DeleteNote)(MediaItem_Take* take, int noteidx);
extern void (*MIDI_Sort)(MediaItem_Take* take);
extern double (*MIDI_GetProjTimeFromPPQPos)(MediaItem_Take* take, double ppq);
extern double (*MIDI_GetPPQPosFromProjTime)(MediaItem_Take* take, double projtime);
extern double (*MIDI_GetProjQNFromPPQPos)(MediaItem_Take* take, double ppq);
extern double (*MIDI_GetPPQPosFromProjQN)(MediaItem_Take* take, double projqn);

// Markers
extern int (*CountProjectMarkers)(ReaProject* proj, int* num_markers, int* num_regions);
extern bool (*EnumProjectMarkers)(int idx, bool* isrgn, double* pos, double* rgnend, const char** name, int* markrgnindexnumber);
extern bool (*AddProjectMarker)(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx);
extern bool (*DeleteProjectMarker)(ReaProject* proj, int markrgnindexnumber, bool isrgn);

// Routing / Sends
extern int (*GetTrackNumSends)(MediaTrack* tr, int category);
extern double (*GetTrackSendInfo_Value)(MediaTrack* tr, int category, int sendidx, const char* parmname);
extern bool (*SetTrackSendInfo_Value)(MediaTrack* tr, int category, int sendidx, const char* parmname, double newvalue);
extern int (*CreateTrackSend)(MediaTrack* tr, MediaTrack* desttr);
extern bool (*RemoveTrackSend)(MediaTrack* tr, int category, int sendidx);

// Envelopes
extern TrackEnvelope* (*GetTrackEnvelopeByName)(MediaTrack* tr, const char* envname);
extern int (*CountEnvelopePoints)(TrackEnvelope* envelope);
extern bool (*GetEnvelopePoint)(TrackEnvelope* envelope, int ptidx, double* time, double* value,
                                 int* shape, double* tension, bool* selected);
extern int (*InsertEnvelopePoint)(TrackEnvelope* envelope, double time, double value, int shape,
                                   double tension, bool selected, bool* noSort);
extern bool (*DeleteEnvelopePointRange)(TrackEnvelope* envelope, double time_start, double time_end);
extern void (*Envelope_SortPoints)(TrackEnvelope* envelope);

// Undo
extern void (*Undo_BeginBlock)();
extern void (*Undo_EndBlock)(const char* descchange, int extraflags);
extern void (*UpdateArrange)();
extern void (*UpdateTimeline)();

// ExtState (in-memory key-value store)
extern void (*SetExtState)(const char* section, const char* key, const char* value, bool persist);
extern const char* (*GetExtState)(const char* section, const char* key);
extern bool (*HasExtState)(const char* section, const char* key);
extern void (*DeleteExtState)(const char* section, const char* key, bool persist);

// Resolve all function pointers from REAPER's plugin info
// Returns false if critical functions are missing
bool resolve_api(reaper_plugin_info_t* rec);

} // namespace api
} // namespace reaserve
