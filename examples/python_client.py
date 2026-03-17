"""ReaServe Python client example.

Usage:
    python python_client.py
"""

import json
import socket
import struct
import sys


class ReaServeClient:
    def __init__(self, host="localhost", port=9876):
        self.host = host
        self.port = port
        self.sock = None
        self._next_id = 1

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
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
            raise RuntimeError(
                f"RPC error {response['error']['code']}: {response['error']['message']}"
            )

        return response["result"]


def main():
    client = ReaServeClient()
    client.connect()

    # Ping
    result = client.call("ping")
    print(f"Ping: {result}")

    # Get project state
    state = client.call("project.get_state")
    print(f"\nProject: {state['bpm']} BPM, {state['track_count']} tracks")
    for track in state.get("tracks", []):
        print(f"  Track {track['index']}: {track['name']} ({track['volume_db']} dB)")

    # Get transport state
    transport = client.call("transport.get_state")
    print(f"\nTransport: {transport['state_name']} at {transport['cursor_position']:.2f}s")

    # Add a track
    result = client.call("track.add", {"name": "New Track"})
    print(f"\nAdded track at index {result['index']}")

    # List markers
    markers = client.call("marker.list")
    print(f"\nMarkers: {markers['marker_count']}, Regions: {markers['region_count']}")

    client.close()


if __name__ == "__main__":
    main()
