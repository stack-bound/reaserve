# Development Guide

## Prerequisites

- CMake 3.15+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

For a debug build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

The plugin binary is output as:
- Linux: `build/reaper_reaserve.so`
- macOS: `build/reaper_reaserve.dylib`
- Windows: `build/reaper_reaserve.dll`

## Running Tests

Tests use plain `assert()` — no external test framework required.

```bash
# Run all tests via CTest
ctest --test-dir build --output-on-failure

# Or run individual test binaries directly
./build/test_json_rpc
./build/test_command_queue
./build/test_framing
```

To disable building tests:

```bash
cmake -B build -DREASERVE_BUILD_TESTS=OFF
```

## Versioning

The version is defined in a single place: the `project(VERSION ...)` line in `CMakeLists.txt`.

At configure time, CMake generates `build/generated/version.h` from the template `src/version.h.in`. This header provides:

```c++
#define REASERVE_VERSION "0.1.0"        // full version string
#define REASERVE_VERSION_MAJOR 0
#define REASERVE_VERSION_MINOR 1
#define REASERVE_VERSION_PATCH 0
```

The version appears in:
- The Reaper console on plugin load (`ReaServe v0.1.0: TCP server started...`)
- The `ping` JSON-RPC response (`{"pong": true, "version": "0.1.0"}`)

**To bump the version**, edit the single line in `CMakeLists.txt`:

```cmake
project(reaserve VERSION 0.2.0 LANGUAGES CXX)
```

Then re-run `cmake -B build` to regenerate `version.h`. Update `CHANGELOG.md` manually.

## CI / CD

### CI (`.github/workflows/build.yml`)

Runs on every push and PR to `main`. Builds on Linux, macOS, and Windows in parallel, runs tests, and uploads the binaries as workflow artifacts (downloadable from the Actions tab, expire after 90 days).

### Releases (`.github/workflows/release.yml`)

Triggered by pushing a `v*` tag. Builds and tests all three platforms, then creates a GitHub Release with platform-specific binaries attached. The release body is extracted from `CHANGELOG.md` automatically.

### Making a Release

```
# 1. Bump version in CMakeLists.txt
#    project(reaserve VERSION 0.2.0 LANGUAGES CXX)

# 2. Update CHANGELOG.md — move items from [staging] to the new version heading

# 3. Commit
git add CMakeLists.txt CHANGELOG.md
git commit -m "Release v0.2.0"

# 4. Tag and push
git tag v0.2.0
git push origin main --tags
```

The tag push triggers the release workflow. Once it completes, a GitHub Release named "ReaServe v0.2.0" will appear with `reaper_reaserve-linux.so`, `reaper_reaserve-macos.dylib`, and `reaper_reaserve-windows.dll` attached for download.

## Project Structure

```
CMakeLists.txt              # Build system — single source of truth for version
src/
  main.cpp                  # Plugin entry point (ReaperPluginEntry)
  version.h.in              # Version header template (CMake substitutes values)
  tcp_server.{h,cpp}        # TCP listener, per-client threads, length-prefixed framing
  json_rpc.{h,cpp}          # JSON-RPC 2.0 parsing and response construction
  command_queue.{h,cpp}     # Thread-safe queue bridging TCP threads to main thread
  command_registry.{h,cpp}  # Method name -> handler dispatch
  reaper_api.{h,cpp}        # REAPER API function pointer resolution
  config.{h,cpp}            # INI config loading (reaserve.ini)
  undo.{h,cpp}              # Undo block helpers
  commands/                 # One file per command group (ping, tracks, fx, etc.)
tests/                      # Unit tests (assert-based, no framework)
vendor/                     # Third-party headers (nlohmann/json)
examples/                   # Client examples in Python, Node.js, Go
```

## Architecture

All REAPER API calls must happen on the main thread. The plugin uses a producer-consumer pattern:

1. TCP client sends a JSON-RPC request
2. `TcpServer` parses it on a background thread and pushes a `PendingCommand` (with a `std::promise`) onto the `CommandQueue`
3. A REAPER timer callback (`~30ms`) pops commands from the queue and dispatches them via `CommandRegistry`
4. The handler calls REAPER's C API, returns a result
5. The promise is fulfilled, unblocking the TCP thread which sends the response

## Adding a New Command

1. Create `src/commands/your_command.cpp`
2. Implement a `register_your_command(CommandRegistry&)` function
3. Add the source file to `CMakeLists.txt` in the `add_library` list
4. Add the forward declaration and registration call in `src/main.cpp`
5. Document the method in `PROTOCOL.md`
