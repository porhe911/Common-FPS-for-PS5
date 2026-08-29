cmake_minimum_required(VERSION 3.20)
project(CommonFPS_PS5 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# RC8 remains a one-file diagnostic binary. It uses only fork + sparse FreeBSD
# sysctl process snapshots. No etaHEN source, shsrv, renderer, ptrace, auth
# helpers, FPS backend, or networking is linked.
add_executable(common_fps_controller
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp")
set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_RC8_Sysctl_Sparse.elf")

target_compile_options(common_fps_controller PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -ffunction-sections -fdata-sections)
target_link_options(common_fps_controller PRIVATE -Wl,--gc-sections)
target_link_libraries(common_fps_controller PRIVATE
    kernel_sys SceLibcInternal)

add_custom_command(TARGET common_fps_controller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_controller>"
            "${ROOT}/dist/Common_FPS_RC8_Sysctl_Sparse.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_RC8_Sysctl_Sparse.elf"
            "${ROOT}/dist/Common_FPS_RC8_Sysctl_Sparse_etaHEN.plugin"
            --title-id CFPS00908 --version 9.08)
