cmake_minimum_required(VERSION 3.16)

project(os_course_design C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(oslab
    src/filesystem.c
    src/memory.c
    src/scheduler.c
    src/sync_demo.c
)
target_include_directories(oslab PUBLIC include)
target_link_libraries(oslab PUBLIC Threads::Threads)

add_executable(oslab_app src/main.c)
target_link_libraries(oslab_app PRIVATE oslab)

add_executable(oslab_tests tests/test_core.c)
target_link_libraries(oslab_tests PRIVATE oslab)

enable_testing()
add_test(NAME core COMMAND oslab_tests)
