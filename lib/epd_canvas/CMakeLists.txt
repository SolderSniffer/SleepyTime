# lib/epd_canvas/CMakeLists.txt
#
# Software framebuffer and layout engine. No Zephyr dependencies.

if(NOT TARGET epd_canvas)
    add_library(epd_canvas STATIC epd_canvas.c)

    target_include_directories(epd_canvas PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_options(epd_canvas PRIVATE
        -Wall -Wextra -Wshadow -Wstrict-prototypes
    )
endif()