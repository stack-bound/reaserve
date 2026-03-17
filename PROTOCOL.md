# ReaServe Protocol Reference

ReaServe communicates via **JSON-RPC 2.0** over **TCP** with **length-prefixed framing**.

## Connection

- Default port: `9876` (configurable in `reaserve.ini`)
- Default bind: `0.0.0.0`

## Framing

Each message is prefixed with a **4-byte big-endian uint32** indicating the payload length in bytes, followed by the UTF-8 JSON payload.

```
[4 bytes: length][N bytes: JSON payload]
```

## JSON-RPC 2.0

### Request

```json
{"jsonrpc": "2.0", "id": 1, "method": "ping", "params": {}}
```

### Response (success)

```json
{"jsonrpc": "2.0", "id": 1, "result": {"pong": true, "version": "0.1.0"}}
```

### Response (error)

```json
{"jsonrpc": "2.0", "id": 1, "error": {"code": -32601, "message": "Method not found"}}
```

## Error Codes

| Code | Meaning |
|------|---------|
| -32700 | Parse error (malformed JSON) |
| -32600 | Invalid request (missing fields) |
| -32601 | Method not found |
| -32602 | Invalid params |
| -32603 | Internal error |
| -32000 | REAPER API error |
| -32001 | Lua execution error |
| -32002 | Timeout (main thread didn't process in time) |

---

## Methods

### `ping`

Health check.

**Params:** none

**Result:**
```json
{"pong": true, "version": "0.1.0"}
```

---

### `lua.execute`

Execute arbitrary Lua code in REAPER.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `code` | string | yes | Lua source code |

**Result:**
```json
{"success": true}
```

---

### `lua.execute_and_read`

Execute Lua code that writes JSON to a file, then return the file contents.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `code` | string | yes | Lua source code (should write JSON to `state_path`) |
| `state_path` | string | yes | Path where Lua writes its output |

**Result:** The parsed JSON from the state file.

---

### `project.get_state`

Get current project state.

**Params:** none

**Result:**
```json
{
  "bpm": 120.0,
  "time_sig_num": 4,
  "time_sig_denom": 4,
  "track_count": 3,
  "cursor_position": 5.2,
  "play_state": 0,
  "project_length": 120.0,
  "tracks": [
    {
      "index": 0,
      "name": "Drums",
      "volume_db": -3.0,
      "pan": 0.0,
      "mute": false,
      "solo": false,
      "record_arm": false,
      "fx_count": 2,
      "item_count": 4
    }
  ]
}
```

---

### `transport.get_state`

Get transport state.

**Params:** none

**Result:**
```json
{
  "play_state": 1,
  "state_name": "playing",
  "cursor_position": 12.5,
  "is_playing": true,
  "is_paused": false,
  "is_recording": false
}
```

---

### `transport.control`

Control transport.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `action` | string | yes | `play`, `stop`, `pause`, or `record` |

**Result:**
```json
{"success": true, "action": "play"}
```

---

### `transport.set_cursor`

Move the edit cursor.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `position` | number | yes | Time in seconds |
| `moveview` | boolean | no | Move arrange view (default: true) |
| `seekplay` | boolean | no | Seek playback (default: true) |

**Result:**
```json
{"success": true, "position": 10.0}
```

---

### `track.add`

Add a new track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | no | Track name |
| `index` | integer | no | Insert position (default: end) |

**Result:**
```json
{"success": true, "index": 3, "track_count": 4}
```

---

### `track.remove`

Remove a track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `index` | integer | yes | Track index |

**Result:**
```json
{"success": true, "track_count": 2}
```

---

### `track.set_property`

Set a track property.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `index` | integer | yes | Track index |
| `property` | string | yes | One of: `name`, `volume_db`, `pan`, `mute`, `solo`, `record_arm` |
| `value` | any | yes | New value (type depends on property) |

**Result:**
```json
{"success": true}
```

---

### `items.get_selected`

Get all selected media items.

**Params:** none

**Result:**
```json
{
  "count": 1,
  "items": [
    {
      "index": 0,
      "position": 2.0,
      "length": 4.0,
      "mute": false,
      "selected": true,
      "take_name": "audio.wav",
      "source_type": "WAVE",
      "track_index": 0
    }
  ]
}
```

---

### `item.list`

List all items, optionally filtered by track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | no | Filter to specific track |

**Result:**
```json
{
  "count": 5,
  "items": [...]
}
```

---

### `item.move`

Move a media item.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | yes | Item index within track |
| `position` | number | yes | New position in seconds |

---

### `item.resize`

Resize a media item.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | yes | Item index within track |
| `length` | number | yes | New length in seconds |

---

### `item.split`

Split a media item.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | yes | Item index within track |
| `position` | number | yes | Split position in seconds |

---

### `item.delete`

Delete a media item.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | yes | Item index within track |

---

### `fx.get_parameters`

Get FX parameters.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `fx_index` | integer | yes | FX index on track |

**Result:**
```json
{
  "fx_name": "VST: ReaEQ (Cockos)",
  "enabled": true,
  "param_count": 16,
  "parameters": [
    {
      "index": 0,
      "name": "Band 1 Frequency",
      "value": 0.5,
      "min": 0.0,
      "max": 1.0,
      "formatted": "1000 Hz"
    }
  ]
}
```

---

### `fx.add`

Add FX to a track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `fx_name` | string | yes | FX name (e.g. "ReaEQ") |
| `position` | integer | no | FX chain position (default: -1 = end) |

**Result:**
```json
{"success": true, "fx_index": 0}
```

---

### `fx.remove`

Remove FX from a track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `fx_index` | integer | yes | FX index |

---

### `fx.set_parameter`

Set an FX parameter value.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `fx_index` | integer | yes | FX index |
| `param_index` | integer | yes | Parameter index |
| `value` | number | yes | New value (normalized 0-1) |

---

### `fx.enable` / `fx.disable`

Enable or disable (bypass) an FX.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `fx_index` | integer | yes | FX index |

---

### `midi.get_notes`

Get MIDI notes from an item.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | yes | Item index |

**Result:**
```json
{
  "note_count": 4,
  "cc_count": 0,
  "notes": [
    {
      "index": 0,
      "pitch": 60,
      "velocity": 100,
      "channel": 0,
      "start_ppq": 0.0,
      "end_ppq": 960.0,
      "start_time": 0.0,
      "end_time": 1.0,
      "start_qn": 0.0,
      "end_qn": 1.0,
      "selected": false,
      "muted": false
    }
  ]
}
```

---

### `midi.insert_notes`

Insert MIDI notes. Creates a new MIDI item if `item_index` is not provided.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `item_index` | integer | no | Existing item (creates new if omitted) |
| `start_time` | number | no | Start time for new item (default: 0) |
| `end_time` | number | no | End time for new item (default: start + 4) |
| `notes` | array | yes | Array of note objects |

Each note object:
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `pitch` | integer | no | MIDI note number (default: 60) |
| `velocity` | integer | no | Velocity (default: 100) |
| `channel` | integer | no | MIDI channel (default: 0) |
| `start_ppq`/`end_ppq` | number | * | Position in PPQ ticks |
| `start_qn`/`end_qn` | number | * | Position in quarter notes |
| `start_time`/`end_time` | number | * | Position in seconds |

*One pair of start/end is required (ppq, qn, or time).

**Result:**
```json
{"success": true, "notes_inserted": 4}
```

---

### `marker.list`

List all markers and regions.

**Params:** none

**Result:**
```json
{
  "marker_count": 3,
  "region_count": 1,
  "markers": [
    {"index": 1, "position": 0.0, "name": "Intro"},
    {"index": 2, "position": 30.0, "name": "Verse"}
  ],
  "regions": [
    {"index": 1, "position": 0.0, "end_position": 30.0, "name": "Section A"}
  ]
}
```

---

### `marker.add`

Add a marker or region.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `position` | number | yes | Position in seconds |
| `name` | string | no | Marker name |
| `index` | integer | no | Desired index (-1 = auto) |
| `is_region` | boolean | no | Create region (default: false) |
| `end_position` | number | no | Region end position |

---

### `marker.remove`

Remove a marker or region.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `index` | integer | yes | Marker/region index |
| `is_region` | boolean | no | Remove region (default: false) |

---

### `routing.list_sends`

List sends/receives for a track.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `category` | integer | no | 0=sends, 1=receives, -1=hw outputs (default: 0) |

**Result:**
```json
{
  "track_index": 0,
  "category": 0,
  "count": 1,
  "sends": [
    {
      "index": 0,
      "volume": 1.0,
      "pan": 0.0,
      "mute": false,
      "dest_channel": 0,
      "src_channel": 0
    }
  ]
}
```

---

### `routing.add_send`

Create a send between tracks.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `src_track` | integer | yes | Source track index |
| `dest_track` | integer | yes | Destination track index |
| `volume` | number | no | Send volume |
| `pan` | number | no | Send pan |

**Result:**
```json
{"success": true, "send_index": 0}
```

---

### `routing.remove_send`

Remove a send.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `send_index` | integer | yes | Send index |
| `category` | integer | no | Category (default: 0) |

---

### `envelope.list`

List envelope automation points.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `envelope_name` | string | yes | Envelope name (e.g. "Volume", "Pan") |

**Result:**
```json
{
  "envelope_name": "Volume",
  "point_count": 3,
  "points": [
    {"index": 0, "time": 0.0, "value": 1.0, "shape": 0, "tension": 0.0, "selected": false}
  ]
}
```

---

### `envelope.add_point`

Add an automation point.

**Params:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `track_index` | integer | yes | Track index |
| `envelope_name` | string | yes | Envelope name |
| `time` | number | yes | Time in seconds |
| `value` | number | yes | Envelope value |
| `shape` | integer | no | Point shape (default: 0 = linear) |
| `tension` | number | no | Tension (default: 0.0) |

**Result:**
```json
{"success": true, "point_index": 0}
```
