# Plan: REAPER Remote API — Standalone C++ Plugin + GoReap Integration

## Context

GoReap currently controls REAPER via a file bridge: Go writes Lua to `/tmp/ai_payload.lua`, triggers execution via REAPER's web remote HTTP API, and polls `/tmp/reaper_state.json` for results. This requires users to enable REAPER's HTTP server, create a custom action, and mount shared temp directories.

We're replacing this with a **standalone C++ REAPER extension plugin** that exposes REAPER's full API over TCP via JSON-RPC 2.0. This is a **separate open-source project** — not embedded in GoReap — because:

1. **Nothing like it exists.** Every REAPER automation project (MCP servers, OSC bridges, reapy wrappers) independently reinvents clunky bridges. A proper plugin with a clean protocol fills a real gap.
2. **Language-agnostic.** Any app in any language can connect — Python, Go, Node.js, Rust, even a web UI. GoReap becomes just one client.
3. **Different audiences.** The plugin serves the entire REAPER developer community. GoReap serves music producers using AI.
4. **Natural separation.** Different languages (C++ vs Go), different build systems (CMake vs `go build`), different release cycles (tied to REAPER versions vs AI features).

### Project Structure

| Repository | Language | Purpose |
|------------|----------|---------|
| **`reaper-remote`** (new, standalone) | C++ | REAPER extension plugin. TCP server exposing JSON-RPC API. The REAPER community project. |
| **`goreap`** (existing) | Go | AI music assistant. Adds `PluginBridge` as a TCP client that speaks the plugin's protocol. |

The contract between them is the **JSON-RPC protocol spec** — a documented set of methods, params, and response formats. GoReap implements a client; anyone else can write their own.

---

## Abstraction Strategy (Addressing the "Too Many Low-Level Calls" Concern)

Three tiers, from highest to lowest abstraction:

| Tier | What | Client Sends | Example |
|------|-------|------------|---------|
| **Tier 1: Structured reads** | Pre-built queries implemented in C++ | Just the method name | `project.get_state` returns BPM, tracks, etc. |
| **Tier 2: Structured mutations** | High-level ops implemented in C++ | Method + simple params | `track.add(name="Bass", index=2)` |
| **Tier 3: Raw Lua escape hatch** | Arbitrary Lua execution | Full Lua code string | `lua.execute` for edge cases |

The plugin handles the 30+ REAPER C API calls internally. Clients (including GoReap's LLM) work with semantic operations. The escape hatch remains for anything the structured commands don't cover.

### How This Helps GoReap's LLM Specifically

**Today (file bridge):** The LLM has 7 MCP tools. For the 6 pre-built tools (reads + MIDI insert + transport), it writes zero Lua. But for everything else — adding tracks, setting volumes, adding FX, routing, markers, envelopes — the LLM must write raw REAPER Lua using functions like `GetMediaTrackInfo_Value(track, "D_VOL")`, `InsertTrackAtIndex(idx, 1)`, `TrackFX_AddByName(track, "ReaEQ", false, -1)`.

**With the plugin:** The LLM gets ~25+ structured MCP tools: `add_track(name="Bass")`, `set_track_property(index=0, property="volume_db", value=-6)`, `add_fx(track_index=0, fx_name="ReaEQ")`. All REAPER C API complexity is buried inside the plugin. The LLM never sees it.

---

## Architecture Overview

```
Any Client (GoReap, Python script, Node.js app, etc.)
  ↓ JSON-RPC 2.0 over TCP (length-prefixed framing)
reaper-remote plugin (C++, loaded by REAPER)
  ├─ TCP server (background thread)
  ├─ Command queue (thread-safe)
  ├─ Timer callback (REAPER main thread, ~30ms)
  └─ REAPER C API calls
```

For GoReap specifically:
```
LLM (via OpenCode)
  ↓ MCP tool calls (JSON-RPC over stdin/stdout)
MCP Server (Go, in-process)
  ↓ bridge.Command("track.add", {"name": "Bass"})
PluginBridge (Go, TCP client)  ← lives in goreap repo
  ↓ JSON-RPC 2.0 over TCP
reaper-remote plugin (C++)     ← lives in reaper-remote repo
  ↓ REAPER C API
REAPER DAW
```

### Why This Threading Model?

REAPER's C API is **NOT thread-safe** — almost all functions must be called from the main thread. The plugin's TCP server runs on a background thread. The solution is a standard producer-consumer pattern:

1. Background thread receives JSON-RPC request, creates a `PendingCommand` with a `std::promise<string>`, pushes to queue
2. Timer callback (registered via `plugin_register("timer", fn)`) runs on REAPER's main thread at ~30ms intervals
3. Timer dequeues command, calls REAPER API, serializes result, fulfills the promise
4. Background thread was waiting on the future, gets the result, sends JSON-RPC response over TCP

The ~30ms timer interval introduces at most 30ms latency per command — imperceptible for user-initiated operations.

### Protocol Design Decisions

**Why TCP?** REAPER runs on the user's desktop. Clients may run in Docker, on other machines, or locally. TCP works everywhere. Unix sockets require shared filesystem.

**Why not HTTP?** Adds overhead (headers, parsing) with no benefit for a point-to-point connection. Raw TCP with length-prefixed framing is simpler, faster, and needs no HTTP library in C++.

**Why JSON-RPC 2.0?** Minimal (4-page spec), well-understood, request IDs for free, maps naturally to command dispatch.

**Why length-prefixed framing?** Lua code in `lua.execute` contains newlines. 4-byte big-endian uint32 length prefix + UTF-8 JSON payload. No delimiter scanning, no edge cases.

---

# Part 1: reaper-remote (Standalone C++ Plugin)

## Project Structure

```
reaper-remote/
├── CMakeLists.txt
├── LICENSE                       # MIT
├── README.md                     # User-facing: install, protocol docs, examples
├── PROTOCOL.md                   # JSON-RPC method reference (the contract)
├── reaper_plugin.h               # REAPER SDK header (public domain, single file)
├── vendor/
│   └── nlohmann/json.hpp         # Single-header JSON library (MIT)
├── src/
│   ├── main.cpp                  # ReaperPluginEntry, timer callback, API resolution
│   ├── tcp_server.cpp/.h         # Background TCP server (length-prefixed framing)
│   ├── command_queue.cpp/.h      # Thread-safe queue (mutex + condition_variable)
│   ├── command_registry.cpp/.h   # Method name → handler dispatch
│   ├── reaper_api.cpp/.h         # REAPER function pointer resolution + null checks
│   ├── undo.cpp/.h               # Undo block wrapper for mutation commands
│   └── commands/
│       ├── ping.cpp/.h           # Health check
│       ├── lua.cpp/.h            # lua.execute, lua.execute_and_read
│       ├── project.cpp/.h        # project.get_state
│       ├── tracks.cpp/.h         # track.add, track.remove, track.set_property, etc.
│       ├── items.cpp/.h          # items.get_selected, item.list, item.move, etc.
│       ├── fx.cpp/.h             # fx.get_parameters, fx.add, fx.remove, etc.
│       ├── midi.cpp/.h           # midi.insert_notes, midi.get_notes, etc.
│       ├── transport.cpp/.h      # transport.control, transport.set_cursor
│       ├── markers.cpp/.h        # marker.add, marker.remove, marker.list
│       ├── routing.cpp/.h        # routing.add_send, routing.list_sends, etc.
│       └── envelope.cpp/.h       # envelope.add_point, envelope.list
├── tests/
│   ├── test_json_rpc.cpp         # JSON-RPC parsing unit tests
│   ├── test_command_queue.cpp    # Thread safety tests
│   ├── test_framing.cpp          # Length-prefixed framing tests
│   └── integration/
│       └── test_client.py        # Python integration test client
├── examples/
│   ├── python_client.py          # Example: control REAPER from Python
│   ├── node_client.js            # Example: control REAPER from Node.js
│   └── go_client.go              # Example: control REAPER from Go
└── .github/
    └── workflows/
        └── build.yml             # Cross-platform CI (Windows/macOS/Linux)
```

## Plugin Implementation Phases

### Phase 1: Foundation — TCP Server + Lua Execution

**Goal:** Plugin loads in REAPER, accepts TCP connections, executes Lua commands. Enough for GoReap to use as a drop-in bridge replacement.

#### `main.cpp` — Entry Point

```cpp
// The ONE function REAPER looks for when loading the plugin
extern "C" int ReaperPluginEntry(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
    if (!rec) {
        // rec == nullptr means REAPER is unloading the plugin
        shutdown_tcp_server();
        return 0;
    }

    // Resolve REAPER API function pointers
    if (!resolve_api(rec)) return 0;  // Failed to get required functions

    // Register timer callback (runs on main thread)
    rec->Register("timer", (void*)timer_callback);

    // Read config (port, bind address) from reaper_remote.ini
    auto config = load_config();

    // Start TCP server on background thread
    start_tcp_server(config.bind, config.port);

    return 1; // Success
}
```

Timer callback:
```cpp
void timer_callback() {
    // Process up to N commands per tick to avoid blocking REAPER's UI
    for (int i = 0; i < 5; i++) {
        auto cmd = command_queue.try_pop();
        if (!cmd) break;

        auto result = command_registry.dispatch(cmd->method, cmd->params);
        cmd->response.set_value(result);  // Unblocks the TCP thread
    }
}
```

#### `reaper_api.cpp` — Function Pointer Resolution

```cpp
// Typedefs from reaper_plugin.h
static int (*CountTracks)(ReaProject*);
static MediaTrack* (*GetTrack)(ReaProject*, int);
static double (*GetMediaTrackInfo_Value)(MediaTrack*, const char*);
// ... ~50 more for all commands we need

bool resolve_api(reaper_plugin_info_t* rec) {
    *(void**)&CountTracks = rec->GetFunc("CountTracks");
    *(void**)&GetTrack = rec->GetFunc("GetTrack");
    // ... resolve all needed functions
    // Return false if any critical function is null (old REAPER version)
}
```

#### `commands/lua.cpp` — Lua Execution

REAPER's C API has no "eval Lua string" function. The established pattern in the extension community:

```cpp
json handle_lua_execute(const json& params) {
    std::string code = params["code"];

    // 1. Write Lua to temp file
    std::string tmp_path = get_temp_path("reaper_remote_XXXXXX.lua");
    write_file(tmp_path, code);

    // 2. Register as a ReaScript action (returns command ID)
    int cmd_id = AddRemoveReaScript(true, 0, tmp_path.c_str(), true);

    // 3. Execute on the main thread (we're already on main thread in timer callback)
    Main_OnCommandEx(cmd_id, 0, nullptr);

    // 4. Unregister and clean up
    AddRemoveReaScript(false, 0, tmp_path.c_str(), true);
    remove(tmp_path.c_str());

    return {{"success", true}};
}
```

For `lua.execute_and_read`, the caller's Lua code writes JSON to a state file. The plugin reads it back:

```cpp
json handle_lua_execute_and_read(const json& params) {
    std::string code = params["code"];
    std::string state_path = params["state_path"];

    // Delete stale state file
    remove(state_path.c_str());

    // Execute Lua (same pattern as above)
    execute_lua(code);

    // Read state file written by the Lua script
    std::string result = read_file(state_path);
    remove(state_path.c_str());

    return json::parse(result);
}
```

#### Protocol — JSON-RPC 2.0

```
→ {"jsonrpc":"2.0","id":1,"method":"ping","params":{}}
← {"jsonrpc":"2.0","id":1,"result":{"pong":true,"version":"0.1.0"}}

→ {"jsonrpc":"2.0","id":2,"method":"lua.execute","params":{"code":"reaper.Main_OnCommand(1007,0)"}}
← {"jsonrpc":"2.0","id":2,"result":{"success":true}}

→ {"jsonrpc":"2.0","id":3,"method":"project.get_state","params":{}}
← {"jsonrpc":"2.0","id":3,"result":{"bpm":120,"track_count":8,"tracks":[...]}}
```

#### Plugin Configuration

`reaper_remote.ini` in REAPER's resource path (auto-created with defaults):
```ini
[reaper-remote]
port=9876
bind=0.0.0.0
```

### Phase 2: Structured Read Commands

**Goal:** Native C++ implementations of common queries. No Lua needed for reads.

#### `commands/project.cpp` — `project.get_state`

```cpp
json handle_project_get_state(const json& params) {
    double bpm, bpi;
    GetProjectTimeSignature2(nullptr, &bpm, &bpi);

    int num_tracks = CountTracks(nullptr);
    double cursor = GetCursorPosition();
    int play_state = GetPlayState();

    json tracks = json::array();
    for (int i = 0; i < num_tracks; i++) {
        MediaTrack* track = GetTrack(nullptr, i);
        if (!track) continue;

        char name_buf[256];
        GetSetMediaTrackInfo_String(track, "P_NAME", name_buf, false);

        double vol = GetMediaTrackInfo_Value(track, "D_VOL");
        double vol_db = (vol > 0) ? 20.0 * log10(vol) : -150.0;

        tracks.push_back({
            {"index", i},
            {"name", std::string(name_buf)},
            {"volume_db", round(vol_db * 10) / 10},
            {"pan", GetMediaTrackInfo_Value(track, "D_PAN")},
            {"mute", (bool)GetMediaTrackInfo_Value(track, "B_MUTE")},
            {"solo", (bool)GetMediaTrackInfo_Value(track, "I_SOLO")},
            {"record_arm", (bool)GetMediaTrackInfo_Value(track, "I_RECARM")},
            {"fx_count", TrackFX_GetCount(track)},
            {"item_count", CountTrackMediaItems(track)}
        });
    }

    return {
        {"bpm", bpm},
        {"time_sig_num", (int)bpi},
        {"time_sig_denom", 4},
        {"track_count", num_tracks},
        {"cursor_position", cursor},
        {"play_state", play_state},
        {"tracks", tracks}
    };
}
```

#### `commands/items.cpp` — `items.get_selected`

Direct C API calls returning selected item metadata (track, take, source, position, length).

#### `commands/fx.cpp` — `fx.get_parameters`

Takes `track_id` and `fx_index`, returns FX name, parameter names/values/ranges.

### Phase 3: Structured Mutation Commands

**Goal:** High-level write operations. All mutations auto-wrap in undo blocks inside C++.

#### Sub-phase 3a: Tracks + Transport

| Command | Params | Description |
|---------|--------|-------------|
| `track.add` | `{name?, index?}` | Add track, optional name/position. Auto undo block. |
| `track.remove` | `{index}` | Delete track by index. Auto undo block. |
| `track.set_property` | `{index, property, value}` | Set volume_db/pan/mute/solo/record_arm/name. Auto undo block. |
| `transport.control` | `{action}` | play/stop/record/rewind via command IDs |
| `transport.set_cursor` | `{position}` | Move playhead |

#### Sub-phase 3b: FX Operations

| Command | Params | Description |
|---------|--------|-------------|
| `fx.add` | `{track_index, fx_name, position?}` | Add plugin by name (e.g. "ReaEQ") |
| `fx.remove` | `{track_index, fx_index}` | Remove FX |
| `fx.set_parameter` | `{track_index, fx_index, param_index, value}` | Set parameter value |
| `fx.enable` / `fx.disable` | `{track_index, fx_index}` | Toggle bypass |

#### Sub-phase 3c: Items + MIDI

| Command | Params | Description |
|---------|--------|-------------|
| `item.move` | `{track_index, item_index, position}` | Move item |
| `item.resize` | `{track_index, item_index, length}` | Resize item |
| `item.split` | `{track_index, item_index, position}` | Split item |
| `item.delete` | `{track_index, item_index}` | Delete item |
| `item.list` | `{track_index?}` | List all items |
| `midi.insert_notes` | `{track_index, start_measure, notes}` | Insert MIDI notes natively |
| `midi.get_notes` | `{track_index, item_index}` | Read MIDI notes from item |

#### Sub-phase 3d: Markers + Routing + Envelopes

| Command | Params | Description |
|---------|--------|-------------|
| `marker.add` | `{position, name, color?}` | Add project marker |
| `marker.remove` | `{index}` | Remove marker |
| `marker.list` | — | List all markers |
| `routing.add_send` | `{src_track, dest_track, volume?, pan?}` | Create send |
| `routing.remove_send` | `{track_index, send_index}` | Remove send |
| `routing.list_sends` | `{track_index}` | List sends |
| `envelope.add_point` | `{track_index, envelope_name, time, value, shape?}` | Add automation point |
| `envelope.list` | `{track_index, envelope_name}` | List envelope points |

### Phase 4: Polish + Distribution

**Cross-platform build:** CMake 3.15+, Windows (MSVC), macOS (clang, x64 + arm64), Linux (gcc). GitHub Actions CI matrix.

**Release artifacts:** `reaper_remote-{platform}-{arch}.{dll,dylib,so}`

**User installation:**
1. Download one file for your platform
2. Copy to `REAPER/UserPlugins/`
   - Windows: `%APPDATA%\REAPER\UserPlugins\`
   - macOS: `~/Library/Application Support/REAPER/UserPlugins/`
   - Linux: `~/.config/REAPER/UserPlugins/`
3. Restart REAPER
4. Connect from any TCP client on port 9876

**PROTOCOL.md** — The key deliverable for the community. Documents every JSON-RPC method, params, response format, error codes. This is the contract that clients implement against.

---

# Part 2: GoReap Integration (Changes to This Repo)

## Go Plugin Bridge (`internal/reaper/plugin_bridge.go`)

New file implementing the existing `ReaperBridge` interface via TCP:

```go
type PluginConfig struct {
    Host           string        // Default: "localhost"
    Port           int           // Default: 9876
    ConnectTimeout time.Duration // Default: 5s
    CommandTimeout time.Duration // Default: 10s
}

type PluginBridge struct {
    cfg    PluginConfig
    mu     sync.Mutex
    conn   net.Conn
    nextID atomic.Uint64
}

func NewPluginBridge(cfg PluginConfig) *PluginBridge { ... }

// --- ReaperBridge interface (backward compatible) ---

func (b *PluginBridge) Execute(code string) error {
    _, err := b.Command("lua.execute", map[string]interface{}{"code": code})
    return err
}

func (b *PluginBridge) ExecuteAndRead(code string) (json.RawMessage, error) {
    return b.Command("lua.execute_and_read", map[string]interface{}{
        "code":       code,
        "state_path": "/tmp/goreap/reaper_state.json",
    })
}

func (b *PluginBridge) HealthCheck() error {
    _, err := b.Command("ping", nil)
    return err
}

// --- PluginCommander interface (new, for structured commands) ---

func (b *PluginBridge) Command(method string, params map[string]interface{}) (json.RawMessage, error) {
    // 1. Serialize JSON-RPC request
    // 2. Send with length-prefixed framing over TCP
    // 3. Read length-prefixed response
    // 4. Parse JSON-RPC response, check for error
    // 5. Return result field
}
```

Connection management: lazy connect, exponential backoff reconnection, per-command timeout.

## New Interface (`internal/mcp/reaper_tools.go`)

Keep existing `ReaperBridge` unchanged. Add optional interface:

```go
// PluginCommander is implemented by bridges that support structured commands.
// The plugin bridge implements this; the file bridge does not.
type PluginCommander interface {
    Command(method string, params map[string]interface{}) (json.RawMessage, error)
}
```

`RegisterReaperTools` type-asserts `bridge.(PluginCommander)` to choose implementations:

```go
func RegisterReaperTools(s *Server, bridge ReaperBridge, statePath string) {
    s.RegisterTool(executeLuaMutationTool(bridge))

    if cmd, ok := bridge.(PluginCommander); ok {
        // Plugin mode: use native commands from reaper-remote
        s.RegisterTool(getProjectStatePluginTool(cmd))
        s.RegisterTool(getSelectedItemDataPluginTool(cmd))
        s.RegisterTool(getFXParametersPluginTool(cmd))
        // Phase 3 tools: only available with plugin
        registerPluginMutationTools(s, cmd)
    } else {
        // File bridge fallback: generate Lua
        s.RegisterTool(getProjectStateTool(bridge, statePath))
        s.RegisterTool(getSelectedItemDataTool(bridge, statePath))
        s.RegisterTool(getFXParametersTool(bridge, statePath))
    }

    s.RegisterTool(analyzeAudioTransientsTool()) // Pure Go, no bridge
    s.RegisterTool(insertMIDIPatternTool(bridge))
    s.RegisterTool(transportControlTool(bridge))
}
```

Plugin-mode handlers become trivial:

```go
func getProjectStatePluginTool(cmd PluginCommander) *Tool {
    return &Tool{
        Name:        "get_project_state",
        Description: "Get current REAPER project state...",
        InputSchema: map[string]interface{}{"type": "object", "properties": map[string]interface{}{}},
        Handler: func(ctx context.Context, args map[string]interface{}) (interface{}, error) {
            result, err := cmd.Command("project.get_state", nil)
            if err != nil {
                return nil, fmt.Errorf("failed to get project state: %w", err)
            }
            return string(result), nil
        },
    }
}
```

## New MCP Tools (Plugin Mode Only)

Registered via `registerPluginMutationTools()` when bridge is `PluginCommander`:

| MCP Tool | reaper-remote Command | LLM Description |
|----------|----------------------|-----------------|
| `add_track` | `track.add` | Add a new track with optional name |
| `remove_track` | `track.remove` | Remove a track by index |
| `set_track_property` | `track.set_property` | Set volume, pan, mute, solo, arm, name |
| `add_fx` | `fx.add` | Add FX plugin to track by name |
| `remove_fx` | `fx.remove` | Remove FX from track |
| `set_fx_parameter` | `fx.set_parameter` | Set FX parameter value |
| `enable_fx` / `disable_fx` | `fx.enable` / `fx.disable` | Toggle FX bypass |
| `move_item` | `item.move` | Move media item |
| `resize_item` | `item.resize` | Resize media item |
| `split_item` | `item.split` | Split media item |
| `delete_item` | `item.delete` | Delete media item |
| `list_items` | `item.list` | List all items |
| `get_midi_notes` | `midi.get_notes` | Read MIDI notes from item |
| `add_marker` | `marker.add` | Add project marker |
| `remove_marker` | `marker.remove` | Remove marker |
| `list_markers` | `marker.list` | List all markers |
| `add_send` | `routing.add_send` | Create track send |
| `remove_send` | `routing.remove_send` | Remove send |
| `list_sends` | `routing.list_sends` | List sends |
| `add_envelope_point` | `envelope.add_point` | Add automation point |
| `list_envelope_points` | `envelope.list` | List envelope points |

Each is a thin wrapper: validate args → `cmd.Command(method, args)` → return result.

## Config Changes (`internal/config/config.go`)

```go
type ReaperConfig struct {
    Enabled        bool   `yaml:"enabled"`
    Host           string `yaml:"host"`
    Port           int    `yaml:"port"`              // Web remote port (file bridge only)
    BridgeMode     string `yaml:"bridge_mode"`       // "plugin" (default) or "file"
    PluginPort     int    `yaml:"plugin_port"`       // reaper-remote TCP port (default: 9876)
    ActionCommand  string `yaml:"action_command"`    // file bridge only
    PayloadPath    string `yaml:"payload_path"`      // file bridge only
    StatePath      string `yaml:"state_path"`        // file bridge only
    PollIntervalMs int64  `yaml:"poll_interval_ms"`  // file bridge only
    PollTimeoutMs  int64  `yaml:"poll_timeout_ms"`   // file bridge only
    AudioMountPath string `yaml:"audio_mount_path"`
}
```

Updated defaults:
```go
Reaper: ReaperConfig{
    Enabled:        false,
    BridgeMode:     "plugin",
    PluginPort:     9876,
    PollIntervalMs: 50,
    PollTimeoutMs:  5000,
    PayloadPath:    "/tmp/goreap/ai_payload.lua",
    StatePath:      "/tmp/goreap/reaper_state.json",
},
```

New env vars: `REAPER_BRIDGE_MODE`, `REAPER_PLUGIN_PORT`

## Orchestrator Changes (`internal/orchestrator/orchestrator.go`)

Bridge creation (~lines 131-151) becomes a mode switch:

```go
if cfg.Reaper.Enabled {
    var bridge mcp.ReaperBridge
    switch cfg.Reaper.BridgeMode {
    case "file":
        bridge = reaper.New(reaper.Config{
            Host:          cfg.Reaper.Host,
            Port:          cfg.Reaper.Port,
            ActionCommand: cfg.Reaper.ActionCommand,
            PayloadPath:   cfg.Reaper.PayloadPath,
            StatePath:     cfg.Reaper.StatePath,
            PollInterval:  time.Duration(cfg.Reaper.PollIntervalMs) * time.Millisecond,
            PollTimeout:   time.Duration(cfg.Reaper.PollTimeoutMs) * time.Millisecond,
        })
    default: // "plugin"
        bridge = reaper.NewPluginBridge(reaper.PluginConfig{
            Host: cfg.Reaper.Host,
            Port: cfg.Reaper.PluginPort,
        })
    }

    regCfg.Reaper = &mcp.ReaperRegistrationConfig{
        Bridge:    bridge,
        StatePath: cfg.Reaper.StatePath,
    }

    if err := bridge.HealthCheck(); err != nil {
        log.Printf("Warning: REAPER not reachable: %v", err)
    } else {
        log.Printf("REAPER bridge connected (%s mode)", cfg.Reaper.BridgeMode)
    }
}
```

## System Prompt Update (`internal/context/goreap.go`)

Update `GoReapContent` to document all new tools. Key changes:
- Document structured tools as the preferred approach
- Document `execute_lua_mutation` as escape hatch for specialized operations
- Note that structured tools auto-handle undo blocks
- Remove emphasis on LLM needing to know Lua API patterns

---

# Files Summary

## GoReap Files to Modify

| File | Change |
|------|--------|
| `internal/config/config.go` | Add `BridgeMode`, `PluginPort` to `ReaperConfig`; update `Default()`; add env var overrides |
| `internal/reaper/plugin_bridge.go` | **New file** — `PluginBridge` implementing `ReaperBridge` + `PluginCommander` |
| `internal/reaper/plugin_bridge_test.go` | **New file** — Tests with mock TCP server |
| `internal/mcp/reaper_tools.go` | Add `PluginCommander` interface; update `RegisterReaperTools` conditional dispatch |
| `internal/mcp/reaper_plugin_tools.go` | **New file** — Plugin-mode tool implementations |
| `internal/orchestrator/orchestrator.go` | Bridge mode switch (~line 131) |
| `internal/context/goreap.go` | Update `GoReapContent` with new tool docs |

## reaper-remote Files to Create (Separate Repo)

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Build system |
| `PROTOCOL.md` | JSON-RPC method reference — the community contract |
| `reaper_plugin.h` | REAPER SDK header (public domain) |
| `vendor/nlohmann/json.hpp` | JSON library (MIT, single header) |
| `src/main.cpp` | Entry point, timer callback, API resolution |
| `src/tcp_server.cpp/.h` | Background TCP server |
| `src/command_queue.cpp/.h` | Thread-safe producer-consumer queue |
| `src/command_registry.cpp/.h` | Method → handler dispatch |
| `src/reaper_api.cpp/.h` | Function pointer resolution |
| `src/undo.cpp/.h` | Undo block wrapper for mutations |
| `src/commands/ping.cpp/.h` | Health check |
| `src/commands/lua.cpp/.h` | Lua execution |
| `src/commands/project.cpp/.h` | Project state queries |
| `src/commands/tracks.cpp/.h` | Track operations |
| `src/commands/items.cpp/.h` | Item operations |
| `src/commands/fx.cpp/.h` | FX operations |
| `src/commands/midi.cpp/.h` | MIDI operations |
| `src/commands/transport.cpp/.h` | Transport operations |
| `src/commands/markers.cpp/.h` | Marker operations |
| `src/commands/routing.cpp/.h` | Routing operations |
| `src/commands/envelope.cpp/.h` | Envelope operations |
| `examples/python_client.py` | Example Python client |
| `examples/node_client.js` | Example Node.js client |
| `examples/go_client.go` | Example Go client |
| `tests/test_json_rpc.cpp` | Unit tests |
| `tests/test_command_queue.cpp` | Thread safety tests |
| `tests/integration/test_client.py` | Integration test suite |
| `.github/workflows/build.yml` | Cross-platform CI |

---

# Verification

1. **reaper-remote Phase 1:** `ping` and `lua.execute` work from a Python test client
2. **GoReap Phase 1:** All 7 existing MCP tools work identically over plugin bridge — diff JSON outputs against file bridge
3. **reaper-remote Phase 2:** Native read commands return identical JSON to Lua-generated equivalents
4. **reaper-remote Phase 3:** Each mutation tested: create track → verify, set volume → verify, add FX → verify
5. **All phases:** Existing Go tests pass unchanged — file bridge untouched
6. **Plugin unit tests:** JSON-RPC parsing, command queue thread safety, framing encode/decode

# Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Plugin crashes REAPER | High | Null-check all API returns, catch C++ exceptions in timer callback, extensive testing |
| REAPER API missing (old version) | Medium | Check function pointers at load, require REAPER v6.0+, log missing functions |
| TCP connection drops | Low | Go bridge auto-reconnects with exponential backoff |
| Thread safety bugs | Medium | Simple mutex + deque + condition_variable pattern, RAII lock guards |
| Protocol drift between repos | Medium | `PROTOCOL.md` is the source of truth; version the protocol; GoReap pins a minimum plugin version |

# Migration Path

1. Both bridge modes coexist in GoReap. Default is `plugin` for new installs. Existing users keep `file` until they install the plugin.
2. `bridge_mode: file` logs a deprecation warning.
3. Eventually: file bridge removal in a major GoReap version. Or keep forever as zero-cost fallback.

# User Setup (End State)

**REAPER side:** Download `reaper_remote` binary → copy to `UserPlugins/` → restart REAPER. Done.

**GoReap side:**
```yaml
reaper:
  enabled: true
  host: "192.168.1.100"
  plugin_port: 9876
```

**Eliminated:** web remote setup, custom action creation, `/tmp/goreap` mounts, action command config.
