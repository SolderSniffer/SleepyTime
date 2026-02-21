# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sleepytime VERSION 0.1.0)

# ── Include paths ─────────────────────────────────────────────────────────────
# App sources (service headers) and lib headers all made available here.
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_time
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/epd_canvas
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_sm
)

# ── Sources ───────────────────────────────────────────────────────────────────
# Lib sources are compiled directly into the app target by the Zephyr build
# system, avoiding the separate static library / toolchain mismatch that
# occurs when add_subdirectory pulls in libs already built for x86_64 by
# `make test`. The host test build uses its own CMakeLists (tests/) which
# compiles the same sources into separate host-native static libraries.
target_sources(app PRIVATE
    main.c
    display_service.c
    time_service.c
    power_service.c
    ble_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_time/watch_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/epd_canvas/epd_canvas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_sm/watch_sm.c
)