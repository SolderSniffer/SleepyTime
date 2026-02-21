# tests/CMakeLists.txt
#
# Host-side Unity unit test suite. Built and run by `make test` — no Zephyr
# toolchain or hardware required.
#
# Invoked by the Makefile as:
#   cmake -S tests -B build/test -G Ninja \
#         -DUNITY_DIR=/opt/unity \
#         -DSLEEPYTIME_ROOT=<absolute path to repo root>
#   ninja -C build/test
#   ctest --test-dir build/test --output-on-failure

cmake_minimum_required(VERSION 3.20)
project(sleepytime_tests C)

# ── Repo root ─────────────────────────────────────────────────────────────────
if(NOT DEFINED SLEEPYTIME_ROOT)
    get_filename_component(SLEEPYTIME_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        ABSOLUTE)
    message(STATUS "SLEEPYTIME_ROOT not set — inferred as: ${SLEEPYTIME_ROOT}")
endif()

set(SLEEPYTIME_LIB_DIR "${SLEEPYTIME_ROOT}/lib")

if(NOT EXISTS "${SLEEPYTIME_LIB_DIR}")
    message(FATAL_ERROR
        "lib/ directory not found at '${SLEEPYTIME_LIB_DIR}'.\n"
        "Pass the repo root explicitly: cmake -DSLEEPYTIME_ROOT=<path> ...")
endif()

# ── Unity ─────────────────────────────────────────────────────────────────────
if(NOT DEFINED UNITY_DIR)
    message(FATAL_ERROR "UNITY_DIR must be set to the Unity installation path.")
endif()

set(UNITY_SRC ${UNITY_DIR}/src/unity.c)
if(NOT EXISTS ${UNITY_SRC})
    message(FATAL_ERROR "Unity source not found at ${UNITY_SRC}.")
endif()

add_library(unity STATIC ${UNITY_SRC})
target_include_directories(unity PUBLIC ${UNITY_DIR}/src)

# ── Lib targets — added ONCE here, shared by all test subdirectories ──────────
# Each lib's own CMakeLists defines the target and its include paths.
# Test subdirectories simply link against these targets by name.
add_subdirectory(${SLEEPYTIME_LIB_DIR}/watch_time  lib/watch_time)
add_subdirectory(${SLEEPYTIME_LIB_DIR}/epd_canvas  lib/epd_canvas)
add_subdirectory(${SLEEPYTIME_LIB_DIR}/watch_sm    lib/watch_sm)

# ── Test suites ───────────────────────────────────────────────────────────────
enable_testing()

add_subdirectory(test_watch_time)
add_subdirectory(test_epd_canvas)
add_subdirectory(test_watch_sm)