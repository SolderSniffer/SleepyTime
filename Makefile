# =============================================================================
# SleepyTime — Project Makefile
# =============================================================================
#
# This Makefile is the single source of truth for build, lint, test, and dev
# environment commands. Both developers and CI call the same targets so that
# "it works on my machine" is structurally impossible.
#
# Prerequisites (inside the dev container — use `make shell` to enter it):
#   west, cmake, ninja, clang-format, clang-tidy, arm-zephyr-eabi-gcc, Unity
#
# Common developer workflows:
#   make shell          — drop into the hermetic dev container (interactive)
#   make build-debug    — build firmware with debug configuration
#   make build-release  — build firmware with release/optimised configuration
#   make format         — auto-format all C sources in-place
#   make lint           — check formatting + run clang-tidy (CI gate)
#   make test           — compile and run host-side unit tests
#   make clean          — remove all build artefacts
#   make help           — print this summary
#
# CI pipeline calls these targets in order:
#   make lint → make build-debug → make build-release → make test
#
# =============================================================================

# ── Shell and safety flags ────────────────────────────────────────────────────
SHELL        := /bin/bash
.SHELLFLAGS  := -euo pipefail -c
.DEFAULT_GOAL := help

# ── Board / SoC target (nRF54L15 DK) ─────────────────────────────────────────
BOARD        ?= xiao_nrf54l15/nrf54l15/cpuapp

# ── Build output directories ──────────────────────────────────────────────────
BUILD_DEBUG   := build/debug
BUILD_RELEASE := build/release
BUILD_TEST    := build/test

# ── Source directories scanned by lint/format targets ─────────────────────────
SRC_DIRS     := app drivers lib
C_SOURCES    := $(shell find $(SRC_DIRS) -name '*.c' -o -name '*.h' 2>/dev/null)

# ── Dev container image ───────────────────────────────────────────────────────
# Reference by digest (not tag) for reproducibility. Keep in sync with
# devcontainer.json and .github/workflows/ci.yml.
DEVENV_IMAGE ?= ghcr.io/<owner>/sleepytime-devenv@sha256:<digest-goes-here>

# Docker run command used by the `shell` and `ci-*` targets.
# - Mounts the repo root to /workspace
# - Mounts the host ccache directory for fast incremental builds
# - Passes through BOARD so overrides work inside the container
DOCKER_RUN := docker run --rm \
    -v "$(CURDIR)":/workspace \
    -v sleepytime-ccache:/root/.ccache \
    -e BOARD="$(BOARD)" \
    -e CCACHE_DIR=/root/.ccache \
    -w /workspace

# ── Colours (suppressed when not a TTY, e.g. in CI logs) ─────────────────────
RESET  := $(shell tput sgr0 2>/dev/null || true)
BOLD   := $(shell tput bold 2>/dev/null || true)
GREEN  := $(shell tput setaf 2 2>/dev/null || true)
YELLOW := $(shell tput setaf 3 2>/dev/null || true)
CYAN   := $(shell tput setaf 6 2>/dev/null || true)

# =============================================================================
# DEV ENVIRONMENT TARGETS
# =============================================================================

.PHONY: shell
## shell: Start an interactive dev container shell (mounts repo root)
shell:
	@echo "$(CYAN)Entering dev container…$(RESET)"
	$(DOCKER_RUN) -it $(DEVENV_IMAGE) /bin/bash

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
	    -DCONFIG_SIZE_OPTIMIZATIONS=y \
	    -DCONFIG_ASSERT=n \
	    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@echo "$(GREEN)✓ Release build complete:$(RESET) $(BUILD_RELEASE)/zephyr/zephyr.hex"

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
# FLASH TARGET (requires hardware + J-Link/nrfjprog on the host)
# =============================================================================

.PHONY: flash
## flash: Flash the debug build to a connected nRF54L15 DK via west
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

# =============================================================================
# HELP
# =============================================================================

.PHONY: help
## help: Print available targets with descriptions
help:
	@echo ""
	@echo "$(BOLD)SleepyTime Firmware — available make targets$(RESET)"
	@echo ""
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //' | \
	    awk -F': ' '{ printf "  $(CYAN)%-20s$(RESET) %s\n", $$1, $$2 }'
	@echo ""
	@echo "$(YELLOW)Override BOARD to target a different variant:$(RESET)"
	@echo "  make build-debug BOARD=xiao_nrf54l15/nrf54l15/cpuapp"
	@echo ""