# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sleepytime VERSION 0.1.0)

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_time
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/epd_canvas
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_sm
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(app PRIVATE
    # Entry point and wiring
    main.c

    # Services
    display_service.c
    time_service.c
    power_service.c
    ble_service.c
    motion_service.c

    # Wakeup timer implementations
    # Swap wakeup_grtc.c for wakeup_pcf8563.c here to change timer source.
    wakeup_grtc.c

    # Lib sources compiled directly into the app target so the Zephyr
    # build system applies the ARM toolchain. See lib/*/CMakeLists.txt
    # for the host test build which compiles these as separate static libs.
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_time/watch_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/epd_canvas/epd_canvas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/watch_sm/watch_sm.c
)
