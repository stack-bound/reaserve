//go:build integration

// Package integration contains end-to-end tests for the ReaServe plugin.
//
// Prerequisites:
//   - REAPER running with the ReaServe plugin loaded
//   - A new, blank project open (File > New Project)
//   - No tracks, items, or markers present
//
// Run with:
//
//	cd tests/integration && go test -tags integration -v -count=1 .
//
// Set REASERVE_ADDR to override the default "localhost:9876".
package integration

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"math"
	"net"
	"os"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------

type client struct {
	conn   net.Conn
	nextID atomic.Uint64
}

type rpcRequest struct {
	JSONRPC string `json:"jsonrpc"`
	ID      uint64 `json:"id"`
	Method  string `json:"method"`
	Params  any    `json:"params"`
}

type rpcError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

type rpcResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      uint64          `json:"id"`
	Result  json.RawMessage `json:"result,omitempty"`
	Error   *rpcError       `json:"error,omitempty"`
}

func dial(t *testing.T) *client {
	t.Helper()
	addr := os.Getenv("REASERVE_ADDR")
	if addr == "" {
		addr = "localhost:9876"
	}
	conn, err := net.DialTimeout("tcp", addr, 5*time.Second)
	if err != nil {
		t.Fatalf("cannot connect to ReaServe at %s: %v", addr, err)
	}
	conn.SetDeadline(time.Now().Add(30 * time.Second))
	return &client{conn: conn}
}

func (c *client) close() { c.conn.Close() }

// call sends a JSON-RPC request and returns the parsed result.
// On RPC error it returns (nil, rpcError).
func (c *client) call(method string, params any) (json.RawMessage, *rpcError) {
	if params == nil {
		params = map[string]any{}
	}
	id := c.nextID.Add(1)
	data, err := json.Marshal(rpcRequest{JSONRPC: "2.0", ID: id, Method: method, Params: params})
	if err != nil {
		panic(err)
	}

	hdr := make([]byte, 4)
	binary.BigEndian.PutUint32(hdr, uint32(len(data)))
	if _, err := c.conn.Write(append(hdr, data...)); err != nil {
		panic(fmt.Sprintf("write: %v", err))
	}

	if _, err := c.conn.Read(hdr); err != nil {
		panic(fmt.Sprintf("read header: %v", err))
	}
	length := binary.BigEndian.Uint32(hdr)
	buf := make([]byte, length)
	for n := uint32(0); n < length; {
		r, err := c.conn.Read(buf[n:])
		if err != nil {
			panic(fmt.Sprintf("read body: %v", err))
		}
		n += uint32(r)
	}

	var resp rpcResponse
	if err := json.Unmarshal(buf, &resp); err != nil {
		panic(fmt.Sprintf("unmarshal: %v\nraw: %s", err, buf))
	}
	if resp.Error != nil {
		return nil, resp.Error
	}
	return resp.Result, nil
}

// callOK is call() that fails the test on RPC error.
func (c *client) callOK(t *testing.T, method string, params any) json.RawMessage {
	t.Helper()
	res, rpcErr := c.call(method, params)
	if rpcErr != nil {
		t.Fatalf("%s returned error %d: %s", method, rpcErr.Code, rpcErr.Message)
	}
	return res
}

// unmarshal is a helper to decode JSON into a map.
func unmarshal(t *testing.T, raw json.RawMessage) map[string]any {
	t.Helper()
	var m map[string]any
	if err := json.Unmarshal(raw, &m); err != nil {
		t.Fatalf("unmarshal: %v\nraw: %s", err, raw)
	}
	return m
}

// num extracts a float64 from a map, failing the test if missing.
func num(t *testing.T, m map[string]any, key string) float64 {
	t.Helper()
	v, ok := m[key]
	if !ok {
		t.Fatalf("missing key %q in %v", key, m)
	}
	f, ok := v.(float64)
	if !ok {
		t.Fatalf("key %q is %T, want float64", key, v)
	}
	return f
}

// str extracts a string from a map.
func str(t *testing.T, m map[string]any, key string) string {
	t.Helper()
	v, ok := m[key]
	if !ok {
		t.Fatalf("missing key %q in %v", key, m)
	}
	s, ok := v.(string)
	if !ok {
		t.Fatalf("key %q is %T, want string", key, v)
	}
	return s
}

// boolean extracts a bool from a map.
func boolean(t *testing.T, m map[string]any, key string) bool {
	t.Helper()
	v, ok := m[key]
	if !ok {
		t.Fatalf("missing key %q in %v", key, m)
	}
	b, ok := v.(bool)
	if !ok {
		t.Fatalf("key %q is %T, want bool", key, v)
	}
	return b
}

// slice extracts a []any from a map.
func slice(t *testing.T, m map[string]any, key string) []any {
	t.Helper()
	v, ok := m[key]
	if !ok {
		t.Fatalf("missing key %q in %v", key, m)
	}
	s, ok := v.([]any)
	if !ok {
		t.Fatalf("key %q is %T, want []any", key, v)
	}
	return s
}

func approx(a, b, tolerance float64) bool {
	return math.Abs(a-b) < tolerance
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

func TestReaServe(t *testing.T) {
	c := dial(t)
	defer c.close()

	// -----------------------------------------------------------------------
	// Ping
	// -----------------------------------------------------------------------
	t.Run("ping", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "ping", nil))
		if !boolean(t, m, "pong") {
			t.Fatal("pong is not true")
		}
		str(t, m, "version") // just assert it exists
	})

	// -----------------------------------------------------------------------
	// Project state — blank project
	// -----------------------------------------------------------------------
	t.Run("project.get_state_blank", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "project.get_state", nil))
		num(t, m, "bpm")
		num(t, m, "cursor_position")
		num(t, m, "play_state")
		tracks := slice(t, m, "tracks")
		if len(tracks) != 0 {
			t.Fatalf("expected 0 tracks in blank project, got %d — please open a new blank project", len(tracks))
		}
	})

	// -----------------------------------------------------------------------
	// Transport
	// -----------------------------------------------------------------------
	t.Run("transport.get_state", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "transport.get_state", nil))
		num(t, m, "play_state")
		str(t, m, "state_name")
		num(t, m, "cursor_position")
		// booleans
		_ = m["is_playing"]
		_ = m["is_paused"]
		_ = m["is_recording"]
	})

	t.Run("transport.control_stop", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "transport.control", map[string]any{"action": "stop"}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	t.Run("transport.set_cursor", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "transport.set_cursor", map[string]any{"position": 5.0}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		if !approx(num(t, m, "position"), 5.0, 0.01) {
			t.Fatalf("cursor position %.3f, want 5.0", num(t, m, "position"))
		}

		// Reset cursor
		c.callOK(t, "transport.set_cursor", map[string]any{"position": 0.0})
	})

	// -----------------------------------------------------------------------
	// Tracks — add two tracks for subsequent tests
	// -----------------------------------------------------------------------
	t.Run("track.add", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.add", map[string]any{"name": "Test Track A"}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		if num(t, m, "index") != 0 {
			t.Fatalf("expected index 0, got %.0f", num(t, m, "index"))
		}
		if num(t, m, "track_count") != 1 {
			t.Fatalf("expected track_count 1, got %.0f", num(t, m, "track_count"))
		}

		// Add second track
		m = unmarshal(t, c.callOK(t, "track.add", map[string]any{"name": "Test Track B"}))
		if num(t, m, "track_count") != 2 {
			t.Fatalf("expected track_count 2, got %.0f", num(t, m, "track_count"))
		}
	})

	// -----------------------------------------------------------------------
	// Track properties
	// -----------------------------------------------------------------------
	t.Run("track.set_property_name", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "name", "value": "Renamed Track",
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify via project state
		state := unmarshal(t, c.callOK(t, "project.get_state", nil))
		tracks := slice(t, state, "tracks")
		tr := tracks[0].(map[string]any)
		if tr["name"] != "Renamed Track" {
			t.Fatalf("track name = %q, want %q", tr["name"], "Renamed Track")
		}
	})

	t.Run("track.set_property_volume", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "volume_db", "value": -6.0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	t.Run("track.set_property_pan", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "pan", "value": 0.5,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	t.Run("track.set_property_mute", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "mute", "value": true,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify
		state := unmarshal(t, c.callOK(t, "project.get_state", nil))
		tracks := slice(t, state, "tracks")
		tr := tracks[0].(map[string]any)
		if tr["mute"] != true {
			t.Fatal("track should be muted")
		}

		// Unmute
		c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "mute", "value": false,
		})
	})

	t.Run("track.set_property_solo", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "solo", "value": true,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		// Unsolo
		c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "solo", "value": false,
		})
	})

	t.Run("track.set_property_record_arm", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "record_arm", "value": true,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		// Disarm
		c.callOK(t, "track.set_property", map[string]any{
			"index": 0, "property": "record_arm", "value": false,
		})
	})

	// -----------------------------------------------------------------------
	// FX — uses track 0
	// -----------------------------------------------------------------------
	t.Run("fx.add", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "fx.add", map[string]any{
			"track_index": 0, "fx_name": "ReaEQ",
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		idx := num(t, m, "fx_index")
		if idx != 0 {
			t.Fatalf("expected fx_index 0, got %.0f", idx)
		}
	})

	t.Run("fx.get_parameters", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "fx.get_parameters", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		str(t, m, "fx_name")
		boolean(t, m, "enabled")
		count := num(t, m, "param_count")
		if count == 0 {
			t.Fatal("expected at least one parameter")
		}
		params := slice(t, m, "parameters")
		if len(params) == 0 {
			t.Fatal("parameters array is empty")
		}
		p0 := params[0].(map[string]any)
		_ = p0["name"]
		_ = p0["value"]
		_ = p0["min"]
		_ = p0["max"]
	})

	t.Run("fx.set_parameter", func(t *testing.T) {
		raw := c.callOK(t, "fx.set_parameter", map[string]any{
			"track_index": 0, "fx_index": 0, "param_index": 0, "value": 0.75,
		})
		m := unmarshal(t, raw)
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	t.Run("fx.disable", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "fx.disable", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify disabled
		params := unmarshal(t, c.callOK(t, "fx.get_parameters", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		if boolean(t, params, "enabled") {
			t.Fatal("FX should be disabled")
		}
	})

	t.Run("fx.enable", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "fx.enable", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify enabled
		params := unmarshal(t, c.callOK(t, "fx.get_parameters", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		if !boolean(t, params, "enabled") {
			t.Fatal("FX should be enabled")
		}
	})

	t.Run("fx.remove", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "fx.remove", map[string]any{
			"track_index": 0, "fx_index": 0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	// -----------------------------------------------------------------------
	// MIDI — insert notes (creates a MIDI item on track 0), then read back
	// -----------------------------------------------------------------------
	t.Run("midi.insert_notes_new_item", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "midi.insert_notes", map[string]any{
			"track_index": 0,
			"start_time":  0.0,
			"end_time":    4.0,
			"notes": []any{
				map[string]any{"pitch": 60, "velocity": 100, "channel": 0, "start_qn": 0.0, "end_qn": 1.0},
				map[string]any{"pitch": 64, "velocity": 80, "channel": 0, "start_qn": 1.0, "end_qn": 2.0},
				map[string]any{"pitch": 67, "velocity": 90, "channel": 0, "start_qn": 2.0, "end_qn": 3.0},
			},
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		if num(t, m, "notes_inserted") != 3 {
			t.Fatalf("expected 3 notes inserted, got %.0f", num(t, m, "notes_inserted"))
		}
	})

	t.Run("midi.get_notes", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "midi.get_notes", map[string]any{
			"track_index": 0, "item_index": 0,
		}))
		if num(t, m, "note_count") != 3 {
			t.Fatalf("expected 3 notes, got %.0f", num(t, m, "note_count"))
		}
		notes := slice(t, m, "notes")
		if len(notes) != 3 {
			t.Fatalf("expected 3 notes in array, got %d", len(notes))
		}

		// Verify first note
		n0 := notes[0].(map[string]any)
		if n0["pitch"].(float64) != 60 {
			t.Fatalf("note 0 pitch = %.0f, want 60", n0["pitch"].(float64))
		}
		if n0["velocity"].(float64) != 100 {
			t.Fatalf("note 0 velocity = %.0f, want 100", n0["velocity"].(float64))
		}
	})

	// -----------------------------------------------------------------------
	// Items — we now have 1 MIDI item on track 0
	// -----------------------------------------------------------------------
	t.Run("item.list", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "item.list", nil))
		if num(t, m, "count") < 1 {
			t.Fatal("expected at least 1 item")
		}
		items := slice(t, m, "items")
		if len(items) < 1 {
			t.Fatal("items array is empty")
		}
	})

	t.Run("item.list_filtered_by_track", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 0}))
		if num(t, m, "count") != 1 {
			t.Fatalf("expected 1 item on track 0, got %.0f", num(t, m, "count"))
		}

		// Track 1 should have no items
		m = unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 1}))
		if num(t, m, "count") != 0 {
			t.Fatalf("expected 0 items on track 1, got %.0f", num(t, m, "count"))
		}
	})

	t.Run("item.move", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "item.move", map[string]any{
			"track_index": 0, "item_index": 0, "position": 2.0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify position changed
		items := unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 0}))
		arr := slice(t, items, "items")
		item := arr[0].(map[string]any)
		if !approx(item["position"].(float64), 2.0, 0.01) {
			t.Fatalf("item position = %.3f, want 2.0", item["position"].(float64))
		}

		// Move back
		c.callOK(t, "item.move", map[string]any{
			"track_index": 0, "item_index": 0, "position": 0.0,
		})
	})

	t.Run("item.resize", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "item.resize", map[string]any{
			"track_index": 0, "item_index": 0, "length": 8.0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify
		items := unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 0}))
		arr := slice(t, items, "items")
		item := arr[0].(map[string]any)
		if !approx(item["length"].(float64), 8.0, 0.01) {
			t.Fatalf("item length = %.3f, want 8.0", item["length"].(float64))
		}
	})

	t.Run("item.split", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "item.split", map[string]any{
			"track_index": 0, "item_index": 0, "position": 4.0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Should now have 2 items on track 0
		items := unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 0}))
		if num(t, items, "count") != 2 {
			t.Fatalf("expected 2 items after split, got %.0f", num(t, items, "count"))
		}
	})

	t.Run("items.get_selected", func(t *testing.T) {
		// Just verify the call works and returns the expected shape
		m := unmarshal(t, c.callOK(t, "items.get_selected", nil))
		_ = num(t, m, "count")
		_ = slice(t, m, "items")
	})

	t.Run("item.delete", func(t *testing.T) {
		// Delete both items (second first to avoid index shift)
		m := unmarshal(t, c.callOK(t, "item.delete", map[string]any{
			"track_index": 0, "item_index": 1,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success deleting item 1")
		}

		m = unmarshal(t, c.callOK(t, "item.delete", map[string]any{
			"track_index": 0, "item_index": 0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success deleting item 0")
		}

		// Verify clean
		items := unmarshal(t, c.callOK(t, "item.list", map[string]any{"track_index": 0}))
		if num(t, items, "count") != 0 {
			t.Fatalf("expected 0 items after delete, got %.0f", num(t, items, "count"))
		}
	})

	// -----------------------------------------------------------------------
	// Markers
	// -----------------------------------------------------------------------
	t.Run("marker.add_and_list", func(t *testing.T) {
		// Add a marker
		m := unmarshal(t, c.callOK(t, "marker.add", map[string]any{
			"position": 10.0, "name": "Verse",
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success adding marker")
		}

		// Add a second marker
		m = unmarshal(t, c.callOK(t, "marker.add", map[string]any{
			"position": 20.0, "name": "Chorus",
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success adding second marker")
		}

		// Add a region
		m = unmarshal(t, c.callOK(t, "marker.add", map[string]any{
			"position": 0.0, "end_position": 10.0, "name": "Intro Section", "is_region": true,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success adding region")
		}

		// List
		list := unmarshal(t, c.callOK(t, "marker.list", nil))
		if num(t, list, "marker_count") < 2 {
			t.Fatalf("expected at least 2 markers, got %.0f", num(t, list, "marker_count"))
		}
		if num(t, list, "region_count") < 1 {
			t.Fatalf("expected at least 1 region, got %.0f", num(t, list, "region_count"))
		}
	})

	t.Run("marker.remove", func(t *testing.T) {
		// Get current markers to find indices
		list := unmarshal(t, c.callOK(t, "marker.list", nil))
		markers := slice(t, list, "markers")
		regions := slice(t, list, "regions")

		// Remove all markers (reverse order to avoid index issues)
		for i := len(markers) - 1; i >= 0; i-- {
			mk := markers[i].(map[string]any)
			idx := int(mk["index"].(float64))
			m := unmarshal(t, c.callOK(t, "marker.remove", map[string]any{
				"index": idx, "is_region": false,
			}))
			if !boolean(t, m, "success") {
				t.Fatalf("failed to remove marker %d", idx)
			}
		}

		// Remove all regions
		for i := len(regions) - 1; i >= 0; i-- {
			rg := regions[i].(map[string]any)
			idx := int(rg["index"].(float64))
			m := unmarshal(t, c.callOK(t, "marker.remove", map[string]any{
				"index": idx, "is_region": true,
			}))
			if !boolean(t, m, "success") {
				t.Fatalf("failed to remove region %d", idx)
			}
		}

		// Verify clean
		list = unmarshal(t, c.callOK(t, "marker.list", nil))
		if num(t, list, "marker_count") != 0 {
			t.Fatalf("expected 0 markers after cleanup, got %.0f", num(t, list, "marker_count"))
		}
		if num(t, list, "region_count") != 0 {
			t.Fatalf("expected 0 regions after cleanup, got %.0f", num(t, list, "region_count"))
		}
	})

	// -----------------------------------------------------------------------
	// Routing — needs 2 tracks (we have track 0 and track 1)
	// -----------------------------------------------------------------------
	t.Run("routing.add_send", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "routing.add_send", map[string]any{
			"src_track": 0, "dest_track": 1,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
		_ = num(t, m, "send_index")
	})

	t.Run("routing.list_sends", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "routing.list_sends", map[string]any{
			"track_index": 0,
		}))
		if num(t, m, "count") < 1 {
			t.Fatal("expected at least 1 send")
		}
		sends := slice(t, m, "sends")
		if len(sends) < 1 {
			t.Fatal("sends array is empty")
		}
		s0 := sends[0].(map[string]any)
		_ = s0["volume"]
		_ = s0["pan"]
		_ = s0["mute"]
	})

	t.Run("routing.list_receives", func(t *testing.T) {
		// Verify the call succeeds with category=1 (receives).
		// REAPER may not always expose receives via GetTrackNumSends(track, 1)
		// depending on version/platform, so we just check the call shape.
		m := unmarshal(t, c.callOK(t, "routing.list_sends", map[string]any{
			"track_index": 1, "category": 1,
		}))
		_ = num(t, m, "count")
		_ = slice(t, m, "sends")
	})

	t.Run("routing.remove_send", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "routing.remove_send", map[string]any{
			"track_index": 0, "send_index": 0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		// Verify
		list := unmarshal(t, c.callOK(t, "routing.list_sends", map[string]any{
			"track_index": 0,
		}))
		if num(t, list, "count") != 0 {
			t.Fatalf("expected 0 sends after remove, got %.0f", num(t, list, "count"))
		}
	})

	// -----------------------------------------------------------------------
	// Envelopes — use Volume envelope on track 0.
	// Fresh tracks have no active envelopes; activate Volume via action.
	// -----------------------------------------------------------------------
	t.Run("envelope.activate_volume", func(t *testing.T) {
		// Fresh tracks have no active envelopes. Select track 0 and
		// run action 40406 ("Show track volume envelope") to activate it.
		c.callOK(t, "lua.execute", map[string]any{
			"code": `
local track = reaper.GetTrack(0, 0)
reaper.SetMediaTrackInfo_Value(track, "I_SELECTED", 1)
reaper.Main_OnCommand(40406, 0)
`,
		})
	})

	t.Run("envelope.add_point", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "envelope.add_point", map[string]any{
			"track_index":   0,
			"envelope_name": "Volume",
			"time":          0.0,
			"value":         0.5,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}

		m = unmarshal(t, c.callOK(t, "envelope.add_point", map[string]any{
			"track_index":   0,
			"envelope_name": "Volume",
			"time":          2.0,
			"value":         1.0,
			"shape":         0,
			"tension":       0.0,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success adding second point")
		}
	})

	t.Run("envelope.list", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "envelope.list", map[string]any{
			"track_index":   0,
			"envelope_name": "Volume",
		}))
		str(t, m, "envelope_name")
		if num(t, m, "point_count") < 2 {
			t.Fatalf("expected at least 2 points, got %.0f", num(t, m, "point_count"))
		}
		points := slice(t, m, "points")
		if len(points) < 2 {
			t.Fatalf("expected at least 2 points in array, got %d", len(points))
		}
		p0 := points[0].(map[string]any)
		_ = p0["time"]
		_ = p0["value"]
		_ = p0["shape"]
	})

	// -----------------------------------------------------------------------
	// Lua
	// -----------------------------------------------------------------------
	t.Run("lua.execute", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "lua.execute", map[string]any{
			"code": `reaper.ShowConsoleMsg("ReaServe integration test\n")`,
		}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success")
		}
	})

	t.Run("lua.execute_and_read", func(t *testing.T) {
		m := unmarshal(t, c.callOK(t, "lua.execute_and_read", map[string]any{
			"code": `reaserve_output('{"hello": "world", "num": 42}')`,
		}))
		if str(t, m, "hello") != "world" {
			t.Fatalf("hello = %q, want %q", m["hello"], "world")
		}
		if num(t, m, "num") != 42 {
			t.Fatalf("num = %.0f, want 42", num(t, m, "num"))
		}
	})

	t.Run("lua.execute_and_read_complex", func(t *testing.T) {
		// Use Lua to query something from REAPER and return it
		code := `
local track_count = reaper.CountTracks(0)
local bpm, bpi = reaper.GetProjectTimeSignature2(0)
reaserve_output('{"track_count": ' .. track_count .. ', "bpm": ' .. bpm .. '}')
`
		m := unmarshal(t, c.callOK(t, "lua.execute_and_read", map[string]any{
			"code": code,
		}))
		if num(t, m, "track_count") != 2 {
			t.Fatalf("track_count = %.0f, want 2", num(t, m, "track_count"))
		}
		if num(t, m, "bpm") <= 0 {
			t.Fatal("bpm should be > 0")
		}
	})

	t.Run("lua.execute_and_read_error", func(t *testing.T) {
		_, rpcErr := c.call("lua.execute_and_read", map[string]any{
			"code": `error("something went wrong")`,
		})
		if rpcErr == nil {
			t.Fatal("expected RPC error for Lua runtime error")
		}
		if !strings.Contains(rpcErr.Message, "something went wrong") {
			t.Fatalf("error message %q should contain 'something went wrong'", rpcErr.Message)
		}
	})

	t.Run("lua.execute_and_read_no_output", func(t *testing.T) {
		_, rpcErr := c.call("lua.execute_and_read", map[string]any{
			"code": `local x = 1 + 1`,
		})
		if rpcErr == nil {
			t.Fatal("expected RPC error when reaserve_output() not called")
		}
		if !strings.Contains(rpcErr.Message, "reaserve_output") {
			t.Fatalf("error message %q should mention reaserve_output", rpcErr.Message)
		}
	})

	// -----------------------------------------------------------------------
	// Error cases
	// -----------------------------------------------------------------------
	t.Run("error_method_not_found", func(t *testing.T) {
		_, rpcErr := c.call("nonexistent.method", nil)
		if rpcErr == nil {
			t.Fatal("expected RPC error")
		}
		if rpcErr.Code != -32601 {
			t.Fatalf("error code = %d, want -32601", rpcErr.Code)
		}
	})

	t.Run("error_missing_params", func(t *testing.T) {
		_, rpcErr := c.call("track.remove", map[string]any{})
		if rpcErr == nil {
			t.Fatal("expected RPC error for missing params")
		}
	})

	t.Run("error_invalid_track_index", func(t *testing.T) {
		_, rpcErr := c.call("track.remove", map[string]any{"index": 999})
		if rpcErr == nil {
			t.Fatal("expected RPC error for invalid track index")
		}
	})

	// -----------------------------------------------------------------------
	// Cleanup — remove all tracks to leave project blank
	// -----------------------------------------------------------------------
	t.Run("cleanup", func(t *testing.T) {
		// Remove tracks in reverse order
		m := unmarshal(t, c.callOK(t, "track.remove", map[string]any{"index": 1}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success removing track 1")
		}

		m = unmarshal(t, c.callOK(t, "track.remove", map[string]any{"index": 0}))
		if !boolean(t, m, "success") {
			t.Fatal("expected success removing track 0")
		}

		// Reset cursor
		c.callOK(t, "transport.set_cursor", map[string]any{"position": 0.0})

		// Verify blank
		state := unmarshal(t, c.callOK(t, "project.get_state", nil))
		if num(t, state, "track_count") != 0 {
			t.Fatalf("expected 0 tracks after cleanup, got %.0f", num(t, state, "track_count"))
		}
	})
}
