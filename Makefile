# ReaServe Makefile
#
# Usage:
#   make              Build release
#   make debug        Build debug
#   make test         Run unit tests
#   make test-integration  Run integration tests (requires REAPER running with blank project)
#   make install      Copy plugin to REAPER UserPlugins directory
#   make clean        Remove build directory
#   make rebuild      Clean + build
#   make help         Show all targets

ROOT_DIR     := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR    := $(ROOT_DIR)build
BUILD_TYPE   ?= Release
PLUGIN_NAME  := reaper_reaserve

# Detect platform-specific plugin extension and REAPER UserPlugins path
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    PLUGIN_EXT   := .dylib
    USERPLUGINS  ?= $(HOME)/Library/Application Support/REAPER/UserPlugins
else ifeq ($(UNAME),Linux)
    PLUGIN_EXT   := .so
    USERPLUGINS  ?= $(HOME)/.config/REAPER/UserPlugins
else
    PLUGIN_EXT   := .dll
    USERPLUGINS  ?= $(APPDATA)/REAPER/UserPlugins
endif

PLUGIN_BIN := $(BUILD_DIR)/$(PLUGIN_NAME)$(PLUGIN_EXT)

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

.PHONY: all
all: release

.PHONY: configure
configure:
	cmake -S $(ROOT_DIR) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

.PHONY: release
release: BUILD_TYPE = Release
release: configure
	cmake --build $(BUILD_DIR) --config Release

.PHONY: debug
debug: BUILD_TYPE = Debug
debug: configure
	cmake --build $(BUILD_DIR) --config Debug

.PHONY: rebuild
rebuild: clean release

# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------

.PHONY: test
test: release
	ctest --test-dir $(BUILD_DIR) --output-on-failure

.PHONY: test-integration
test-integration:
	cd $(ROOT_DIR)tests/integration && go test -tags integration -v -count=1 .

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

.PHONY: install
install: release
	@mkdir -p "$(USERPLUGINS)"
	cp "$(PLUGIN_BIN)" "$(USERPLUGINS)/"
	@echo "Installed $(PLUGIN_NAME)$(PLUGIN_EXT) to $(USERPLUGINS)"
	@echo "Restart REAPER to load the updated plugin."

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------

.PHONY: help
help:
	@echo "ReaServe build targets:"
	@echo ""
	@echo "  make              Build release (default)"
	@echo "  make debug        Build debug"
	@echo "  make test         Build + run unit tests"
	@echo "  make test-integration  Run Go integration tests (REAPER must be running with blank project)"
	@echo "  make install      Build + copy plugin to REAPER UserPlugins directory"
	@echo "  make clean        Remove build directory"
	@echo "  make rebuild      Clean + release build"
	@echo ""
	@echo "Variables:"
	@echo "  USERPLUGINS=path  Override REAPER UserPlugins directory"
	@echo "  REASERVE_ADDR=h:p Override integration test address (default: localhost:9876)"
