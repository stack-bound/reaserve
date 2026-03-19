"""ReaServe integration tests.

Requires REAPER running with the ReaServe plugin loaded.

Usage:
    python test_client.py [host] [port]
"""

import json
import socket
import struct
import sys
import time


class ReaServeClient:
    def __init__(self, host="localhost", port=9876):
        self.host = host
        self.port = port
        self.sock = None
        self._next_id = 1

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(10)
        self.sock.connect((self.host, self.port))

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def _send(self, data: bytes):
        header = struct.pack(">I", len(data))
        self.sock.sendall(header + data)

    def _recv(self) -> bytes:
        header = self._recv_exact(4)
        length = struct.unpack(">I", header)[0]
        return self._recv_exact(length)

    def _recv_exact(self, n: int) -> bytes:
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError("Connection closed")
            data += chunk
        return data

    def call(self, method: str, params: dict = None) -> dict:
        if params is None:
            params = {}
        req_id = self._next_id
        self._next_id += 1

        request = {
            "jsonrpc": "2.0",
            "id": req_id,
            "method": method,
            "params": params,
        }

        self._send(json.dumps(request).encode("utf-8"))
        response = json.loads(self._recv().decode("utf-8"))

        if "error" in response:
            return {"__error": response["error"]}
        return response["result"]


passed = 0
failed = 0


def test(name, fn):
    global passed, failed
    try:
        fn()
        print(f"  PASS: {name}")
        passed += 1
    except Exception as e:
        print(f"  FAIL: {name} - {e}")
        failed += 1


def main():
    global passed, failed
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9876

    client = ReaServeClient(host, port)
    print(f"Connecting to {host}:{port}...")
    client.connect()
    print("Connected.\n")

    print("Running tests:")

    # Ping
    def test_ping():
        result = client.call("ping")
        assert result["pong"] is True
        assert "version" in result

    test("ping", test_ping)

    # Project state
    def test_project_state():
        result = client.call("project.get_state")
        assert "bpm" in result
        assert "track_count" in result
        assert "tracks" in result
        assert isinstance(result["tracks"], list)

    test("project.get_state", test_project_state)

    # Transport state
    def test_transport_state():
        result = client.call("transport.get_state")
        assert "play_state" in result
        assert "state_name" in result
        assert "cursor_position" in result

    test("transport.get_state", test_transport_state)

    # Track operations
    def test_track_add_remove():
        state = client.call("project.get_state")
        initial_count = state["track_count"]

        # Add track
        result = client.call("track.add", {"name": "Test Track"})
        assert result["success"] is True
        assert result["track_count"] == initial_count + 1

        # Verify
        state = client.call("project.get_state")
        assert state["track_count"] == initial_count + 1

        # Remove it
        result = client.call("track.remove", {"index": result["index"]})
        assert result["success"] is True
        assert result["track_count"] == initial_count

    test("track.add + track.remove", test_track_add_remove)

    # Method not found
    def test_method_not_found():
        result = client.call("nonexistent.method")
        assert "__error" in result
        assert result["__error"]["code"] == -32601

    test("method not found", test_method_not_found)

    # Invalid params
    def test_invalid_params():
        result = client.call("track.remove", {})
        assert "__error" in result

    test("invalid params error", test_invalid_params)

    # Marker operations
    def test_markers():
        result = client.call("marker.add", {"position": 10.0, "name": "Test Marker"})
        assert result["success"] is True

        markers = client.call("marker.list")
        assert markers["marker_count"] > 0

    test("marker.add + marker.list", test_markers)

    # Transport control
    def test_transport_control():
        result = client.call("transport.control", {"action": "stop"})
        assert result["success"] is True

    test("transport.control", test_transport_control)

    # Cursor
    def test_set_cursor():
        result = client.call("transport.set_cursor", {"position": 5.0})
        assert result["success"] is True
        assert result["position"] == 5.0

    test("transport.set_cursor", test_set_cursor)

    # lua.execute_and_read — happy path
    def test_lua_execute_and_read():
        code = 'reaserve_output(\'{"hello": "world", "num": 42}\')'
        result = client.call("lua.execute_and_read", {"code": code})
        assert result["hello"] == "world"
        assert result["num"] == 42

    test("lua.execute_and_read", test_lua_execute_and_read)

    # lua.execute_and_read — Lua error is captured
    def test_lua_execute_and_read_error():
        code = 'error("something went wrong")'
        result = client.call("lua.execute_and_read", {"code": code})
        assert "__error" in result
        assert "something went wrong" in result["__error"]["message"]

    test("lua.execute_and_read error", test_lua_execute_and_read_error)

    # lua.execute_and_read — no reaserve_output() call
    def test_lua_execute_and_read_no_output():
        code = "local x = 1 + 1"
        result = client.call("lua.execute_and_read", {"code": code})
        assert "__error" in result
        assert "reaserve_output" in result["__error"]["message"]

    test("lua.execute_and_read no output", test_lua_execute_and_read_no_output)

    # Item list
    def test_item_list():
        result = client.call("item.list")
        assert "count" in result
        assert "items" in result

    test("item.list", test_item_list)

    # Selected items
    def test_selected_items():
        result = client.call("items.get_selected")
        assert "count" in result
        assert "items" in result

    test("items.get_selected", test_selected_items)

    client.close()

    print(f"\n{passed}/{passed + failed} tests passed.")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
