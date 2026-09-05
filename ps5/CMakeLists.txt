cmake_minimum_required(VERSION 3.20)
project(CommonFPS_PS5 C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set")
endif()

if(NOT COMMON_FPS_ETAHEN_SOURCE)
    message(FATAL_ERROR "COMMON_FPS_ETAHEN_SOURCE is required")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")
set(ETAHEN_ROOT "${COMMON_FPS_ETAHEN_SOURCE}/Source Code")
set(ETAHEN_FPS "${ETAHEN_ROOT}/fps_elf")

# ------------------------------------------------------------------
# Controller/plugin ELF.
# ------------------------------------------------------------------
add_executable(common_fps_controller)

set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf"
)

target_sources(common_fps_controller PRIVATE
    # PARITY TEST20: observer plus the real FW 9.60 VideoOut/DMAP sampler.
    # Renderer, injector, IPC and shutdown recorder are deliberately absent.
    "${ROOT}/src/core/fps_sampler.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"
    "${ROOT}/src/ps5/ps5_platform.cpp"
    "${ROOT}/src/ps5/stable_sampler/process_sysctl.cpp"
    "${ROOT}/src/ps5/stable_sampler/proc_rw_v960.cpp"
    "${ROOT}/src/ps5/stable_sampler/videoout_module.cpp"
)

target_compile_definitions(common_fps_controller PRIVATE
    COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG=1
    COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY=1
)

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_controller PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -fPIE
    -ffunction-sections
    -fdata-sections
    -march=znver2
    -O2
    -Wall
    -Wextra
)

target_link_options(common_fps_controller PRIVATE
    -Wl,--gc-sections
)

target_link_libraries(common_fps_controller PRIVATE
    kernel_sys
    SceLibcInternal
)

# ------------------------------------------------------------------
# Package the controller as one etaHEN .plugin.
# ------------------------------------------------------------------
add_custom_command(
    TARGET common_fps_controller
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:common_fps_controller>"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf"
    COMMAND python3
            "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf"
            "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.plugin"
            --title-id CFPS00039
            --version 1.28
)
