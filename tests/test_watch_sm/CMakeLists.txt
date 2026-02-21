# tests/test_watch_sm/CMakeLists.txt
# lib targets are defined in the parent CMakeLists — just link here.
# watch_time is linked transitively via watch_sm's PUBLIC dependency.

add_executable(test_watch_sm test_watch_sm.c)

target_link_libraries(test_watch_sm PRIVATE unity watch_sm)

target_compile_options(test_watch_sm PRIVATE -Wall -Wextra -Wshadow)

add_test(NAME watch_sm COMMAND test_watch_sm)