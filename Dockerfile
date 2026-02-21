# =============================================================================
# SleepyTime — Hermetic Build Environment
# =============================================================================
#
# Produces a fully self-contained environment for building and testing firmware
# targeting the Nordic nRF54L15 SoC via the nRF Connect SDK (NCS) / Zephyr.
#
# Version pins (update these together when bumping the toolchain):
#   Ubuntu LTS:    24.04 (noble)   — pinned by digest below
#   NCS:           v3.2.1
#   Zephyr SDK:    0.17.4          — minimal bundle + arm-zephyr-eabi toolchain
#   Python:        3.12            — ships with Ubuntu 24.04
#   west:          1.3.0
#   clang-tools:   18              — clang-format + clang-tidy (LLVM APT repo)
#   Unity:         v2.6.0          — host-side unit test framework
#
# Updating the toolchain:
#   1. Change the ARG values below.
#   2. Update the Ubuntu digest (docker pull ubuntu:24.04 && docker inspect … | grep Id).
#   3. Push a PR — the devenv CI workflow rebuilds and pushes a new GHCR image.
#   4. Merge the auto-PR that updates the digest in ci.yml and devcontainer.json.
#
# Local developer shell:
#   make shell
#
# Non-interactive (CI):
#   docker run --rm -v "$(pwd)":/workspace <image>@<digest> make build-debug
#
# =============================================================================

# Pin to a specific Ubuntu 24.04 digest so the base layer never silently drifts.
# Refresh with: docker pull ubuntu:24.04 && docker inspect ubuntu:24.04 --format '{{index .RepoDigests 0}}'
FROM ubuntu:24.04@sha256:72297848456d5d37d1262630108ab308d3e9ec7ed1c3286a32fe09856619a782

# ── Metadata ──────────────────────────────────────────────────────────────────
LABEL org.opencontainers.image.title="sleepytime-devenv"
LABEL org.opencontainers.image.description="Hermetic NCS/Zephyr build env for the SleepyTime smartwatch (nRF54L15)"
LABEL org.opencontainers.image.source="https://github.com/<owner>/sleepytime"

# ── Version ARGs ──────────────────────────────────────────────────────────────
ARG NCS_VERSION=v3.2.1
ARG ZEPHYR_SDK_VERSION=0.17.4
ARG WEST_VERSION=1.3.0
ARG UNITY_VERSION=v2.6.0

# ── Runtime environment ───────────────────────────────────────────────────────
ENV DEBIAN_FRONTEND=noninteractive \
    ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk \
    NCS_DIR=/opt/ncs \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# ── 1. System packages ────────────────────────────────────────────────────────
# Strategy for clang-format/clang-tidy version pinning:
#   Ubuntu 24.04 and its transitive deps (e.g. libsdl2-dev) pull in a newer
#   LLVM (currently 21.x) and register it via update-alternatives. Rather than
#   fighting alternatives, we add the APT repos and install our pinned versions
#   FIRST in an isolated step, then install everything else. After all apt work
#   is done we write wrapper scripts into /usr/local/bin — which precedes
#   /usr/bin on $PATH — that exec the exact versioned binary. Wrapper scripts
#   (unlike symlinks) cannot be silently replaced by a subsequent apt install.
RUN \
    # ── Step A: register external APT repos before any package install ────────
    apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates gnupg wget lsb-release software-properties-common \
    && \
    wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor -o /usr/share/keyrings/llvm-archive-keyring.gpg && \
    printf 'deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] \
http://apt.llvm.org/noble/ llvm-toolchain-noble-18 main\n' \
        > /etc/apt/sources.list.d/llvm.list && \
    wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg && \
    printf 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] \
https://apt.kitware.com/ubuntu/ noble main\n' \
        > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && \
    # ── Step B: install pinned clang tools BEFORE anything that could pull ────
    #            in a newer LLVM as a transitive dep (e.g. libsdl2-dev)
    apt-get install -y --no-install-recommends \
        clang-format-18 \
        clang-tidy-18 \
    && \
    # ── Step C: install the rest of the packages ──────────────────────────────
    apt-get install -y --no-install-recommends \
        build-essential \
        ccache \
        cmake \
        device-tree-compiler \
        file \
        git \
        gperf \
        libsdl2-dev \
        make \
        ninja-build \
        python3 \
        python3-pip \
        python3-venv \
        xz-utils \
    && \
    # ── Step D: write wrapper scripts into /usr/local/bin ─────────────────────
    # Wrapper scripts are used instead of symlinks because apt can overwrite a
    # symlink target via update-alternatives but cannot replace an executable
    # file that was not installed by a package. These will always win on $PATH.
    printf '#!/bin/sh\nexec /usr/bin/clang-format-18 "$@"\n' \
        > /usr/local/bin/clang-format && chmod +x /usr/local/bin/clang-format && \
    printf '#!/bin/sh\nexec /usr/bin/clang-tidy-18 "$@"\n' \
        > /usr/local/bin/clang-tidy   && chmod +x /usr/local/bin/clang-tidy && \
    # ── Step E: clean up ──────────────────────────────────────────────────────
    apt-get clean && rm -rf /var/lib/apt/lists/*

# ── 2. Python tooling (venv keeps system Python pristine) ─────────────────────
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

RUN pip install --no-cache-dir \
        west==${WEST_VERSION} \
        # Pin transitive deps that west and NCS scripts depend on
        PyYAML==6.0.2 \
        packaging==24.1 \
        colorama==0.4.6

# ── 3. Zephyr SDK — minimal bundle + ARM toolchain only ───────────────────────
# The nRF54L15 is a Cortex-M33; we only need arm-zephyr-eabi.
# Using the minimal bundle shaves ~2 GB off the image compared to the full bundle.
RUN SDK_BASE="https://github.com/zephyrproject-rtos/sdk-ng/releases/download" && \
    BUNDLE="zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" && \
    ARM_TC="toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz" && \
    mkdir -p ${ZEPHYR_SDK_INSTALL_DIR} && \
    # Fetch and checksum-verify the bundle and toolchain before extracting
    wget -q "${SDK_BASE}/v${ZEPHYR_SDK_VERSION}/${BUNDLE}" -P /tmp && \
    wget -q "${SDK_BASE}/v${ZEPHYR_SDK_VERSION}/${ARM_TC}" -P /tmp && \
    wget -q "${SDK_BASE}/v${ZEPHYR_SDK_VERSION}/sha256.sum" -P /tmp && \
    cd /tmp && sha256sum --check --ignore-missing sha256.sum && \
    tar -xf /tmp/${BUNDLE} -C ${ZEPHYR_SDK_INSTALL_DIR} --strip-components=1 && \
    tar -xf /tmp/${ARM_TC} -C ${ZEPHYR_SDK_INSTALL_DIR} && \
    # Register the SDK's CMake package and install udev rules
    ${ZEPHYR_SDK_INSTALL_DIR}/setup.sh -t arm-zephyr-eabi -h -c && \
    rm -f /tmp/${BUNDLE} /tmp/${ARM_TC} /tmp/sha256.sum

# ── 4. nRF Connect SDK (west workspace baked into image) ──────────────────────
# Baking NCS in avoids re-fetching ~1 GB of dependencies on every container
# start. Use --depth=1 fetches to minimise image size.
RUN mkdir -p ${NCS_DIR} && \
    cd ${NCS_DIR} && \
    west init -m https://github.com/nrfconnect/sdk-nrf --mr ${NCS_VERSION} && \
    west update --narrow --fetch-opt=--depth=1 && \
    # Export Zephyr CMake package so builds can find it without environment vars
    west zephyr-export && \
    # Install all Python requirements declared by the NCS workspace.
    # NOTE: zephyr/scripts/requirements.txt pulls in the `clang-format` PyPI
    # package which drops a version-varying binary into /opt/venv/bin/clang-format
    # and would shadow our pinned system binary. We uninstall it immediately and
    # replace it with our own wrapper script at the same venv path so the version
    # is always exactly clang-format-18 regardless of what NCS requested.
    pip install --no-cache-dir \
        -r zephyr/scripts/requirements.txt \
        -r nrf/scripts/requirements.txt \
        -r bootloader/mcuboot/scripts/requirements.txt && \
    pip uninstall -y clang-format 2>/dev/null || true && \
    printf '#!/bin/sh\nexec /usr/bin/clang-format-18 "$@"\n' \
        > /opt/venv/bin/clang-format && chmod +x /opt/venv/bin/clang-format

# ── 5. Unity — host-side unit test framework ──────────────────────────────────
RUN git clone --depth 1 --branch ${UNITY_VERSION} \
        https://github.com/ThrowTheSwitch/Unity.git /opt/unity

# ── 6. Final environment wiring ───────────────────────────────────────────────
ENV ZEPHYR_BASE="${NCS_DIR}/zephyr" \
    NRF_DIR="${NCS_DIR}/nrf" \
    UNITY_DIR="/opt/unity" \
    PATH="/opt/venv/bin:${NCS_DIR}/zephyr/scripts:/opt/zephyr-sdk/arm-zephyr-eabi/bin:$PATH"

# Repo root is mounted here at runtime
WORKDIR /workspace

# Interactive shell by default; CI overrides with the make target to run
CMD ["/bin/bash"]