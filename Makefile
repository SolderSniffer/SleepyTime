# =============================================================================
# SleepyTime — Project Makefile
# =============================================================================
#
# This Makefile is the single source of truth for build, lint, test, and dev
# environment commands. Both developers and CI call the same targets so that
# "it works on my machine" is structurally impossible.
#
# Host prerequisites:
#   - Docker Engine (Linux) or Docker Desktop (macOS / Windows)
#   - GNU Make
#
# One-time setup:
#   make image          — build the local dev container image (~15 min)
#
# Common developer workflows:
#   make build-debug    — build firmware with debug configuration
#   make build-release  — build firmware with release/optimised configuration
#   make samples        — list available standalone sample apps
#   make build-sample SAMPLE=blink
#   make flash-sample SAMPLE=blink
#   make format         — auto-format all C sources in-place
#   make lint           — check formatting + run clang-tidy (CI gate)
#   make test           — compile and run host-side unit tests
#   make flash          — flash debug build to connected hardware
#   make shell          — drop into the dev container (to probe SDK files etc.)
#   make clean          — remove all build artefacts
#   make help           — print this summary
#
# All targets that require the toolchain run transparently inside Docker.
# You do not need to run `make shell` or "Reopen in Container" first.
#
# CI pipeline calls these targets in order:
#   make lint → make build-debug → make build-release → make test
#
# =============================================================================

# ── Shell and safety flags ────────────────────────────────────────────────────
SHELL        := /bin/bash
.SHELLFLAGS  := -euo pipefail -c
.DEFAULT_GOAL := help

# ── Board / SoC target ───────────────────────────────────────────────────────
BOARD        ?= xiao_nrf54l15/nrf54l15/cpuapp

# ── DTS overlay ───────────────────────────────────────────────────────────────
# Selects which hardware configuration to build for:
#   sleepytime_proto.overlay  — prototype with discrete modules (default)
#   sleepytime_v1.overlay     — first custom PCB spin (future)
#
# Override at the command line:
#   make build-debug OVERLAY=app/boards/sleepytime_v1.overlay
OVERLAY      ?= app/boards/sleepytime_proto.overlay

# ── Build output directories ──────────────────────────────────────────────────
BUILD_DEBUG   := build/debug
BUILD_RELEASE := build/release
BUILD_TEST    := build/test

# ── Standalone sample app defaults ───────────────────────────────────────────
SAMPLE       ?= blink
SAMPLE_DIR   := samples/$(SAMPLE)
BUILD_SAMPLE := build/samples/$(SAMPLE)

# ── Source directories scanned by lint/format targets ─────────────────────────
SRC_DIRS     := app drivers lib
C_SOURCES    := $(shell find $(SRC_DIRS) -name '*.c' -o -name '*.h' 2>/dev/null)

# ── Dev container image ───────────────────────────────────────────────────────
# Reference by digest (not tag) for reproducibility. Keep in sync with
# devcontainer.json and .github/workflows/ci.yml.
DEVENV_IMAGE ?= sleepytime-devenv:local

# ── Docker run — flags only, image and command are appended at each call site ─
#
# Keeping the image out of DOCKER_RUN_BASE means we can insert extra flags
# (e.g. --privileged for flash) between the base flags and the image without
# any ordering problems. docker run syntax is strictly:
#   docker run [OPTIONS] IMAGE [COMMAND]
#
# - Mounts the repo root to /workspace
# - Mounts a named volume for ccache so incremental builds survive restarts
# - Forwards build-affecting variables so CLI overrides work transparently
#   e.g. `make build-debug BOARD=other_board` does the right thing
# - Sets IN_CONTAINER=1 so the recursive make invocation skips this block
DOCKER_RUN_BASE := docker run --rm \
    -v "$(CURDIR)":/workspace \
    -v sleepytime-ccache:/root/.ccache \
    -e BOARD="$(BOARD)" \
    -e OVERLAY="$(OVERLAY)" \
    -e SAMPLE="$(SAMPLE)" \
    -e CCACHE_DIR=/root/.ccache \
    -e IN_CONTAINER=1 \
    -w /workspace

# ── Colours (suppressed when not a TTY, e.g. in CI logs) ─────────────────────
RESET  := $(shell tput sgr0 2>/dev/null || true)
BOLD   := $(shell tput bold 2>/dev/null || true)
GREEN  := $(shell tput setaf 2 2>/dev/null || true)
YELLOW := $(shell tput setaf 3 2>/dev/null || true)
CYAN   := $(shell tput setaf 6 2>/dev/null || true)

# =============================================================================
# SELF-CONTAINERISATION
# =============================================================================
#
# If we are NOT already inside the dev container, intercept every toolchain
# target and re-invoke it inside Docker transparently. The developer just runs
# `make build-debug` on their host — Docker is an implementation detail.
#
# IN_CONTAINER=1 is set in DOCKER_RUN_BASE above. Inside the container the
# ifndef block is skipped entirely and the real target bodies execute directly.
#
# Targets intentionally kept on the host:
#   image   — runs `docker build`, must be on the host by definition
#   shell   — opens an interactive container session
#   help    — pure make, no toolchain needed
#   samples — pure shell find, no toolchain needed

ifndef IN_CONTAINER

# Standard targets: no special docker flags needed beyond the base.
CONTAINERISED := \
    build-debug \
    build-release \
    build-sample \
    format \
    format-check \
    tidy \
    lint \
    test \
    clean

.PHONY: $(CONTAINERISED)
$(CONTAINERISED):
	$(DOCKER_RUN_BASE) "$(DEVENV_IMAGE)" make $@ \
	    BOARD="$(BOARD)" OVERLAY="$(OVERLAY)" SAMPLE="$(SAMPLE)"

# Flash targets need USB passthrough to reach the CMSIS-DAP debugger.
# --privileged and the /dev/bus/usb mount are intentionally absent from
# DOCKER_RUN_BASE so that CI build/test jobs are never granted host USB access.
.PHONY: flash flash-sample
flash flash-sample:
	$(DOCKER_RUN_BASE) \
	    --privileged \
	    -v /dev/bus/usb:/dev/bus/usb \
	    "$(DEVENV_IMAGE)" make $@ \
	    BOARD="$(BOARD)" OVERLAY="$(OVERLAY)" SAMPLE="$(SAMPLE)"

# =============================================================================
# IMAGE TARGET (host-side only)
# =============================================================================

.PHONY: image
## image: Build the local dev container image (one-time setup, ~15 min)
image:
	@echo "$(BOLD)$(CYAN)[image]$(RESET) Building dev container image…"
	docker build -t sleepytime-devenv:local .
	@echo "$(GREEN)✓ Image built: sleepytime-devenv:local$(RESET)"

# =============================================================================
# SHELL TARGET (host-side only)
# =============================================================================

.PHONY: shell
## shell: Drop into an interactive dev container shell (to probe SDK files etc.)
shell:
	@echo "$(CYAN)Entering dev container…$(RESET)"
	$(DOCKER_RUN_BASE) -it "$(DEVENV_IMAGE)" /bin/bash

else
# =============================================================================
# EVERYTHING BELOW RUNS INSIDE THE CONTAINER
# =============================================================================

# =============================================================================
# BUILD TARGETS
# =============================================================================

.PHONY: build-debug
## build-debug: Build firmware with debug symbols and assertions enabled
build-debug:
	@echo "$(BOLD)$(GREEN)[build-debug]$(RESET) Building for $(BOARD)…"
	west build \
	    --board $(BOARD) \
	    --build-dir $(BUILD_DEBUG) \
	    --pristine=auto \
	    app \
	    -- \
	    -DDTC_OVERLAY_FILE=$(CURDIR)/$(OVERLAY) \
	    -DCONFIG_DEBUG=y \
	    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@echo "$(GREEN)✓ Debug build complete:$(RESET) $(BUILD_DEBUG)/zephyr/zephyr.hex"

.PHONY: build-release
## build-release: Build firmware with size optimisations (matches production)
build-release:
	@echo "$(BOLD)$(GREEN)[build-release]$(RESET) Building for $(BOARD)…"
	west build \
	    --board $(BOARD) \
	    --build-dir $(BUILD_RELEASE) \
	    --pristine=auto \
	    app \
	    -- \
	    -DDTC_OVERLAY_FILE=$(CURDIR)/$(OVERLAY) \
	    -DCONFIG_SIZE_OPTIMIZATIONS=y \
	    -DCONFIG_ASSERT=n \
	    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@echo "$(GREEN)✓ Release build complete:$(RESET) $(BUILD_RELEASE)/zephyr/zephyr.hex"

# =============================================================================
# SAMPLE TARGETS
# =============================================================================

.PHONY: build-sample
## build-sample: Build a sample (make build-sample SAMPLE=blink)
build-sample:
	@if [ ! -d "$(SAMPLE_DIR)" ]; then \
	    echo "$(YELLOW)Sample not found: $(SAMPLE_DIR)$(RESET)"; \
	    echo "Run 'make samples' to list valid values."; \
	    exit 2; \
	fi
	@echo "$(BOLD)$(GREEN)[build-sample]$(RESET) Building sample '$(SAMPLE)' for $(BOARD)…"
	west build \
	    --board $(BOARD) \
	    --build-dir $(BUILD_SAMPLE) \
	    --pristine=auto \
	    --source-dir $(SAMPLE_DIR)
	@echo "$(GREEN)✓ Sample build complete:$(RESET) $(BUILD_SAMPLE)/zephyr/zephyr.hex"

.PHONY: flash-sample
## flash-sample: Flash a sample (make flash-sample SAMPLE=blink)
flash-sample: build-sample
	@echo "$(CYAN)[flash-sample]$(RESET) Flashing sample '$(SAMPLE)'…"
	west flash --build-dir $(BUILD_SAMPLE)

# =============================================================================
# LINT / FORMAT TARGETS
# =============================================================================

.PHONY: format
## format: Auto-format all C sources in-place using clang-format
format:
	@echo "$(CYAN)[format]$(RESET) Formatting $(words $(C_SOURCES)) files…"
	@if [ -z "$(C_SOURCES)" ]; then \
	    echo "$(YELLOW)No C sources found in: $(SRC_DIRS)$(RESET)"; \
	else \
	    clang-format -i --style=file $(C_SOURCES); \
	    echo "$(GREEN)✓ Format complete.$(RESET)"; \
	fi

.PHONY: format-check
## format-check: Fail if any C source is not clang-format compliant (used by CI)
format-check:
	@echo "$(CYAN)[format-check]$(RESET) Checking formatting…"
	@if [ -z "$(C_SOURCES)" ]; then \
	    echo "$(YELLOW)No C sources found — skipping format check.$(RESET)"; \
	else \
	    clang-format --dry-run --Werror --style=file $(C_SOURCES) && \
	    echo "$(GREEN)✓ All files are correctly formatted.$(RESET)"; \
	fi

.PHONY: tidy
## tidy: Run clang-tidy static analysis against the debug compile_commands.json
tidy: $(BUILD_DEBUG)/app/compile_commands.json
	@echo "$(CYAN)[tidy]$(RESET) Running clang-tidy…"
	@if [ -z "$(C_SOURCES)" ]; then \
	    echo "$(YELLOW)No C sources found — skipping tidy.$(RESET)"; \
	else \
	    sed -e 's/-fno-printf-return-value//g' \
	        -e 's/-fno-reorder-functions//g' \
	        -e 's/-mfp16-format=ieee//g' \
	        -e 's/-fno-defer-pop//g' \
	        -e 's/--param=min-pagesize=0//g' \
	        -e 's/-specs=picolibc.specs//g' \
	        $(BUILD_DEBUG)/app/compile_commands.json \
	        > /tmp/compile_commands.json && \
	    clang-tidy \
	        -p /tmp \
	        --warnings-as-errors='*' \
	        $(filter %.c, $(C_SOURCES)) && \
	    echo "$(GREEN)✓ clang-tidy passed.$(RESET)"; \
	fi

# compile_commands.json is generated by build-debug; make sure it exists first.
$(BUILD_DEBUG)/app/compile_commands.json: build-debug

.PHONY: lint
## lint: Run format-check + tidy (the full CI lint gate)
lint: format-check tidy

# =============================================================================
# TEST TARGET
# =============================================================================

.PHONY: test
## test: Compile and run host-side unit tests using Unity
#
# Host-side tests live under tests/ and target the host architecture (x86_64),
# not the MCU. This means they run in the CI container without any hardware.
# The build system links each test suite against the Unity framework at
# /opt/unity (installed in the dev image).
test:
	@echo "$(BOLD)$(GREEN)[test]$(RESET) Building host-side unit tests…"
	@mkdir -p $(BUILD_TEST)
	cmake \
	    -S tests \
	    -B $(BUILD_TEST) \
	    -G Ninja \
	    -DUNITY_DIR=/opt/unity \
	    -DSLEEPYTIME_ROOT=$(CURDIR) \
	    -DCMAKE_BUILD_TYPE=Debug
	ninja -C $(BUILD_TEST)
	@echo "$(CYAN)Running tests…$(RESET)"
	ctest --test-dir $(BUILD_TEST) --output-on-failure
	@echo "$(GREEN)✓ All tests passed.$(RESET)"

# =============================================================================
# FLASH TARGETS
# =============================================================================

.PHONY: flash
## flash: Flash the debug build to a connected nRF54L15 via west
flash: build-debug
	@echo "$(CYAN)[flash]$(RESET) Flashing $(BOARD)…"
	west flash --build-dir $(BUILD_DEBUG)

# =============================================================================
# CLEAN
# =============================================================================

.PHONY: clean
## clean: Remove all build artefacts
clean:
	@echo "$(CYAN)[clean]$(RESET) Removing build directories…"
	rm -rf build/
	@echo "$(GREEN)✓ Clean complete.$(RESET)"

endif
# =============================================================================
# HOST-SIDE TARGETS (no toolchain needed — run on host regardless of context)
# =============================================================================

.PHONY: samples
## samples: List available standalone sample apps under samples/
samples:
	@echo "$(BOLD)Available samples:$(RESET)"
	@if [ -d samples ]; then \
	    find samples -mindepth 1 -maxdepth 1 -type d -printf "  - %f\n" | sort; \
	else \
	    echo "  (none)"; \
	fi

.PHONY: help
## help: Print available targets with descriptions
help:
	@echo ""
	@echo "$(BOLD)SleepyTime Firmware — available make targets$(RESET)"
	@echo ""
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //' | \
	    awk -F': ' '{ printf "  $(CYAN)%-20s$(RESET) %s\n", $$1, $$2 }'
	@echo ""
	@echo "$(YELLOW)Override BOARD or OVERLAY for different hardware:$(RESET)"
	@echo "  make build-debug OVERLAY=app/boards/sleepytime_v1.overlay"
	@echo "  make build-debug BOARD=other_board/variant OVERLAY=app/boards/other.overlay"
	@echo ""