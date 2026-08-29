cmake_minimum_required(VERSION 3.20)
project(CommonFPS_SR7 C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")

add_library(sr7_backend STATIC
    "${ROOT}/src/ps5/legacy_v028b/process_sysctl.cpp"
    "${ROOT}/src/ps5/legacy_v028b/proc_rw_v960.cpp"
    "${ROOT}/src/ps5/legacy_v028b/videoout_counter.cpp"
)

target_include_directories(sr7_backend PUBLIC
    "${ROOT}/src/ps5/legacy_v028b"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(sr7_backend PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -march=znver2
    -O2
    -Wall
    -Wextra
    -Werror
    -ffunction-sections
    -fdata-sections
)

add_executable(common_fps_v028b_sr7
    "${ROOT}/src/ps5/legacy_v028b/sr7_phuf_loopback_probe.cpp"
)

set_target_properties(common_fps_v028b_sr7 PROPERTIES
    OUTPUT_NAME "Common_FPS_SR7_v028b_PHUF_Loopback.elf"
)

target_include_directories(common_fps_v028b_sr7 PRIVATE
    "${ROOT}/src/ps5/legacy_v028b"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_v028b_sr7 PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -fPIE
    -march=znver2
    -O2
    -Wall
    -Wextra
    -Werror
    -ffunction-sections
    -fdata-sections
)

target_link_options(common_fps_v028b_sr7 PRIVATE -Wl,--gc-sections)

target_link_libraries(common_fps_v028b_sr7 PRIVATE
    sr7_backend
    kernel_sys
    SceLibcInternal
    SceNet
)

add_custom_command(
    TARGET common_fps_v028b_sr7 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr7"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr7>"
            "${ROOT}/dist/v028b-sr7/Common_FPS_SR7_v028b_PHUF_Loopback.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr7/Common_FPS_SR7_v028b_PHUF_Loopback.elf"
            "${ROOT}/dist/v028b-sr7/Common_FPS_SR7_v028b_PHUF_Loopback_etaHEN.plugin"
            --title-id CFPS00920 --version 9.20
)
