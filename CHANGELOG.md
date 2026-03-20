# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [staging]



## [0.1.2] - 2026-03-19
### Added
- REAPER ExtState API bindings (`SetExtState`, `GetExtState`, `HasExtState`, `DeleteExtState`).
- Go integration test suite (`tests/integration/reaserve_test.go`) that exercises every method in PROTOCOL.md against a live REAPER instance. Uses `//go:build integration` tag so it is never picked up by normal `go test` runs.
- Makefile with targets: `release`, `debug`, `test`, `test-integration`, `install`, `clean`, `rebuild`, `help`.
- `make install` auto-detects REAPER UserPlugins path per platform (Linux/macOS/Windows).
### Changed
- **Breaking:** `lua.execute_and_read` no longer requires `state_path` parameter. Lua code must now call `reaserve_output(json_string)` to return data instead of writing to a file. The file-based result passing mechanism has been replaced with REAPER's in-memory ExtState API (`SetExtState`/`GetExtState`).
- User Lua code in `lua.execute_and_read` is now wrapped in `pcall()` — Lua runtime errors are captured and returned in the JSON-RPC error response instead of being silently swallowed.
- Updated PROTOCOL.md to reflect new `lua.execute_and_read` interface and document `reaserve_output()`.

## [0.1.1] - 2026-03-17
### Fixed
- Fix REAPER hanging on exit due to client TCP sockets not being shut down during server stop, causing blocking `recv()` calls in client threads to never return
- Unregister timer callback (`-timer`) during plugin unload to prevent use-after-free of destroyed globals
- Drain pending command queue on shutdown so in-flight futures resolve instead of blocking indefinitely

## [0.1.0] - 2026-03-17
### Added
- Initial Build. Artefacts are built for Linux, Windows, and macOS and published to GitHub releases.

# Notes
[Deployment] Notes for deployment
[Added] for new features.
[Changed] for changes in existing functionality.
[Deprecated] for once-stable features removed in upcoming releases.
[Removed] for deprecated features removed in this release.
[Fixed] for any bug fixes.
[Security] to invite users to upgrade in case of vulnerabilities.
[YANKED] Note the emphasis, used for Hotfixes
