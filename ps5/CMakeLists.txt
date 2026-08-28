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
    "${ROOT}/src/ps5/ps5_autoload_backend.cpp"
    "${ROOT}/src/ps5/state_sender.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"

    # GPL etaHEN source dependency for process/module enumeration.
    "${ETAHEN_FPS}/src/proc.cpp"

    # GPL shsrv ptrace + source ELF loader implementation.
    # We call elfldr_debug() under our own attach/detach policy so a failed
    # Common FPS renderer install never SIGKILLs SceShellUI.
    "${COMMON_FPS_SHSRV_SOURCE}/pt.c"
    "${COMMON_FPS_SHSRV_SOURCE}/elfldr.c"
)

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
    "${COMMON_FPS_SHSRV_SOURCE}"
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
    -ffunction-sections
    -fdata-sections
)

target_link_options(common_fps_controller PRIVATE
    -Wl,--gc-sections
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
# Stage B resolves only the Mono/PUI symbols required for Common FPS, finds the
# ShellUI Game container scene and uses PUI Application.Update as the sole
# main-thread rendering callback.  The full etaHEN prx.cpp is not linked.
# ------------------------------------------------------------------
add_executable(common_fps_shellui)

set_target_properties(common_fps_shellui PROPERTIES
    OUTPUT_NAME "Common_FPS_ShellUI_v1.1.0.elf"
)

target_sources(common_fps_shellui PRIVATE
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/wire.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui_bootstrap.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui_entry.cpp"

    # Minimal etaHEN GPL support used by the source-built PUI path.
    "${ETAHEN_FPS}/src/proc.cpp"
    "${ETAHEN_SHELLUI}/src/MonoUtils.cpp"
    "${ETAHEN_SHELLUI}/src/external_symbols.cpp"
    "${ETAHEN_SHELLUI}/src/Detour.cpp"
    "${ETAHEN_SHELLUI}/src/hde64.cpp"
)

target_include_directories(common_fps_shellui PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5/shellui_payload"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
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
    -ffunction-sections
    -fdata-sections
    -Wno-c++11-narrowing
)

target_link_options(common_fps_shellui PRIVATE
    -Wl,--gc-sections
)

target_link_libraries(common_fps_shellui PRIVATE
    kernel_sys
    SceLibcInternal
    SceNet
    SceSysmodule
    SceSystemService
)

# ------------------------------------------------------------------
# Embed the source-built ShellUI companion in the controller.
#
# This makes runtime deployment a single-file operation:
#   etaHEN:  Common_FPS_PS5_etaHEN_v1.1.0.plugin -> /data/etaHEN/plugins/
#   ELF:     Common_FPS_PS5_v1.1.0.elf -> the user's normal ELF/autoload path
#
# No /data/CommonFPS renderer file is required.  The standalone companion is
# still emitted to dist/ for diagnostics and source-build transparency.
# ------------------------------------------------------------------
set(EMBEDDED_SHELLUI_CPP
    "${CMAKE_CURRENT_BINARY_DIR}/generated/commonfps_shellui_blob.cpp"
)

add_custom_command(
    OUTPUT "${EMBEDDED_SHELLUI_CPP}"
    COMMAND python3
            "${ROOT}/tools/embed_binary.py"
            "$<TARGET_FILE:common_fps_shellui>"
            "${EMBEDDED_SHELLUI_CPP}"
    DEPENDS
        common_fps_shellui
        "${ROOT}/tools/embed_binary.py"
    COMMENT "Embedding source-built ShellUI companion into controller"
    VERBATIM
)

set_source_files_properties(
    "${EMBEDDED_SHELLUI_CPP}"
    PROPERTIES GENERATED TRUE
)

target_sources(common_fps_controller PRIVATE
    "${EMBEDDED_SHELLUI_CPP}"
)

# ------------------------------------------------------------------
# Package controller as etaHEN .plugin.
# The renderer is embedded in the controller; the separate renderer artifact
# below is developer/diagnostic output only.
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
