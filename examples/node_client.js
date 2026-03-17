/**
 * ReaServe Node.js client example.
 *
 * Usage:
 *   node node_client.js
 */

const net = require("net");

class ReaServeClient {
  constructor(host = "localhost", port = 9876) {
    this.host = host;
    this.port = port;
    this.socket = null;
    this.nextId = 1;
    this.pending = new Map();
    this.buffer = Buffer.alloc(0);
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection({ host: this.host, port: this.port }, resolve);
      this.socket.on("error", reject);
      this.socket.on("data", (data) => this._onData(data));
    });
  }

  close() {
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }
  }

  call(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = this.nextId++;
      const request = JSON.stringify({ jsonrpc: "2.0", id, method, params });
      const payload = Buffer.from(request, "utf-8");
      const header = Buffer.alloc(4);
      header.writeUInt32BE(payload.length, 0);

      this.pending.set(id, { resolve, reject });
      this.socket.write(Buffer.concat([header, payload]));
    });
  }

  _onData(data) {
    this.buffer = Buffer.concat([this.buffer, data]);

    while (this.buffer.length >= 4) {
      const length = this.buffer.readUInt32BE(0);
      if (this.buffer.length < 4 + length) break;

      const payload = this.buffer.slice(4, 4 + length).toString("utf-8");
      this.buffer = this.buffer.slice(4 + length);

      const response = JSON.parse(payload);
      const pending = this.pending.get(response.id);
      if (pending) {
        this.pending.delete(response.id);
        if (response.error) {
          pending.reject(new Error(`RPC error ${response.error.code}: ${response.error.message}`));
        } else {
          pending.resolve(response.result);
        }
      }
    }
  }
}

async function main() {
  const client = new ReaServeClient();
  await client.connect();

  // Ping
  const ping = await client.call("ping");
  console.log("Ping:", ping);

  // Get project state
  const state = await client.call("project.get_state");
  console.log(`\nProject: ${state.bpm} BPM, ${state.track_count} tracks`);
  for (const track of state.tracks || []) {
    console.log(`  Track ${track.index}: ${track.name} (${track.volume_db} dB)`);
  }

  // Get transport state
  const transport = await client.call("transport.get_state");
  console.log(`\nTransport: ${transport.state_name} at ${transport.cursor_position.toFixed(2)}s`);

  client.close();
}

main().catch(console.error);
