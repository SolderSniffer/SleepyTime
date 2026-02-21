# lib/watch_sm/CMakeLists.txt
#
# Event-driven watch state machine. No Zephyr dependencies.
# watch_time must already be defined as a target before this file is included.

if(NOT TARGET watch_sm)
    add_library(watch_sm STATIC watch_sm.c)

    target_include_directories(watch_sm PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # PUBLIC so consumers of watch_sm also get watch_time headers transitively.
    target_link_libraries(watch_sm PUBLIC watch_time)

    target_compile_options(watch_sm PRIVATE
        -Wall -Wextra -Wshadow -Wstrict-prototypes
    )
endif()