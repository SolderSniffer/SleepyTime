# lib/watch_time/CMakeLists.txt
#
# Pure time-domain logic. No Zephyr dependencies.
# Builds identically whether invoked from the Zephyr app or the host test build.
# The ZEPHYR_BASE guard is kept only to suppress the misleading
# zephyr_library_named() warning — we always use plain add_library here.

if(NOT TARGET watch_time)
    add_library(watch_time STATIC watch_time.c)

    target_include_directories(watch_time PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_options(watch_time PRIVATE
        -Wall -Wextra -Wshadow -Wstrict-prototypes
    )
endif()