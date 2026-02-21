# tests/test_watch_time/CMakeLists.txt
# lib targets are defined in the parent CMakeLists — just link here.

add_executable(test_watch_time test_watch_time.c)

target_link_libraries(test_watch_time PRIVATE unity watch_time)

target_compile_options(test_watch_time PRIVATE -Wall -Wextra -Wshadow)

add_test(NAME watch_time COMMAND test_watch_time)