# tests/test_epd_canvas/CMakeLists.txt
# lib targets are defined in the parent CMakeLists — just link here.

add_executable(test_epd_canvas test_epd_canvas.c)

target_link_libraries(test_epd_canvas PRIVATE unity epd_canvas)

target_compile_options(test_epd_canvas PRIVATE -Wall -Wextra -Wshadow)

add_test(NAME epd_canvas COMMAND test_epd_canvas)