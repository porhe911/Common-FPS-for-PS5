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

if(NOT COMMON_FPS_SHSRV_SOURCE)
    message(FATAL_ERROR "COMMON_FPS_SHSRV_SOURCE is required")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")

set(ETAHEN_ROOT "${COMMON_FPS_ETAHEN_SOURCE}/Source Code")
set(ETAHEN_FPS "${ETAHEN_ROOT}/fps_elf")
set(ETAHEN_SHELLUI "${ETAHEN_ROOT}/shellui")

# ------------------------------------------------------------------
# Controller/plugin ELF.
# ------------------------------------------------------------------
add_executable(common_fps_controller)

set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_PS5_v1.1.0.elf"
)

target_sources(common_fps_controller PRIVATE
    "${ROOT}/src/core/autoload_guard.cpp"
    "${ROOT}/src/core/config.cpp"
    "${ROOT}/src/core/fps_sampler.cpp"
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/lifecycle.cpp"
    "${ROOT}/src/core/scene_guard.cpp"
    "${ROOT}/src/core/wire.cpp"

    "${ROOT}/src/ps5/ps5_platform.cpp"
    "${ROOT}/src/ps5/state_sender.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"

    # GPL etaHEN source dependency for process/module enumeration.
    "${ETAHEN_FPS}/src/proc.cpp"
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
    -march=znver2
    -O2
    -Wall
    -Wextra
)

target_link_libraries(common_fps_controller PRIVATE
    kernel_sys
    SceLibcInternal
    SceSystemService
    SceNet
    SceSysmodule
    SceUserService
    SceNetCtl
)

# ------------------------------------------------------------------
# Source-only ShellUI renderer target.
#
# This target intentionally contains no PHU binary blob. The Common FPS
# renderer source is compiled together with etaHEN's GPL ShellUI support
# source. The exact minimal set can be reduced further after FW 9.60 tests.
# ------------------------------------------------------------------
add_executable(common_fps_shellui)

set_target_properties(common_fps_shellui PROPERTIES
    OUTPUT_NAME "Common_FPS_ShellUI_v1.1.0.elf"
)

target_sources(common_fps_shellui PRIVATE
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/wire.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui.cpp"

    "${ETAHEN_SHELLUI}/src/MonoUtils.cpp"
    "${ETAHEN_SHELLUI}/src/external_symbols.cpp"
    "${ETAHEN_SHELLUI}/src/Detour.cpp"
    "${ETAHEN_SHELLUI}/src/hde64.cpp"
)

target_include_directories(common_fps_shellui PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5/shellui_payload"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_SHELLUI}/include"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_shellui PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -fPIE
    -march=znver2
    -O2
    -Wall
    -Wextra
)

target_link_libraries(common_fps_shellui PRIVATE
    kernel_sys
    SceNet
    SceSysmodule
    SceSystemService
)

# ------------------------------------------------------------------
# Package controller as etaHEN .plugin.
# The renderer ELF remains a source-built companion artifact until the
# controller-side loader embedding step is validated.
# ------------------------------------------------------------------
add_custom_command(
    TARGET common_fps_controller
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:common_fps_controller>"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
    COMMAND python3
            "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
            "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin"
            --title-id CFPS00021
            --version 1.10
)

add_custom_command(
    TARGET common_fps_shellui
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:common_fps_shellui>"
            "${ROOT}/dist/Common_FPS_ShellUI_v1.1.0.elf"
)
