cmake_minimum_required(VERSION 3.20)
project(CommonFPS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(common_fps_core STATIC
    src/core/autoload_guard.cpp
    src/core/scene_guard.cpp
    src/core/app.cpp
    src/core/config.cpp
    src/core/fps_sampler.cpp
    src/core/layout.cpp
    src/core/lifecycle.cpp
    src/core/wire.cpp
)

target_include_directories(common_fps_core PUBLIC include)
target_compile_options(common_fps_core PRIVATE -Wall -Wextra -Wpedantic)

option(COMMON_FPS_BUILD_TESTS "Build host tests" ON)

if(COMMON_FPS_BUILD_TESTS)
    enable_testing()

    add_executable(common_fps_tests
        tests/test_core.cpp
    )

    target_link_libraries(common_fps_tests PRIVATE common_fps_core)
    add_test(NAME common_fps_tests COMMAND common_fps_tests)
endif()


if(COMMON_FPS_BUILD_TESTS)
    add_executable(common_fps_v1_parity_tests
        tests/test_v1_parity.cpp
    )
    target_link_libraries(common_fps_v1_parity_tests PRIVATE common_fps_core)
    add_test(NAME common_fps_v1_parity_tests COMMAND common_fps_v1_parity_tests)
endif()


if(COMMON_FPS_BUILD_TESTS)
    add_executable(common_fps_autoload_tests
        tests/test_autoload_guard.cpp
    )
    target_link_libraries(common_fps_autoload_tests PRIVATE common_fps_core)
    add_test(NAME common_fps_autoload_tests COMMAND common_fps_autoload_tests)
endif()

if(COMMON_FPS_BUILD_TESTS)
    # These tests use assert(), including calls inside assertions. Keep the
    # checks active in Release builds used by CI.
    foreach(common_fps_test_target IN ITEMS
            common_fps_tests
            common_fps_v1_parity_tests
            common_fps_autoload_tests)
        if(MSVC)
            target_compile_options(${common_fps_test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${common_fps_test_target} PRIVATE -UNDEBUG)
        endif()
    endforeach()
endif()
