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

option(
    COMMON_FPS_SLEEP_RECOVERY
    "Build the v1.1.1 candidate with ShellUI PID recovery"
    OFF
)

if(COMMON_FPS_SLEEP_RECOVERY)
    set(COMMON_FPS_RENDERER_OUTPUT "Common_FPS_ShellUI_v1.1.1_sleep_recovery")
    set(COMMON_FPS_CONTROLLER_OUTPUT "Common_FPS_PS5_v1.1.1_SLEEP_RECOVERY.elf")
    set(COMMON_FPS_PLUGIN_OUTPUT "Common_FPS_PS5_etaHEN_v1.1.1_SLEEP_RECOVERY.plugin")
else()
    set(COMMON_FPS_RENDERER_OUTPUT "Common_FPS_ShellUI_v1.1.0")
    set(COMMON_FPS_CONTROLLER_OUTPUT "Common_FPS_PS5_v1.1.0.elf")
    set(COMMON_FPS_PLUGIN_OUTPUT "Common_FPS_PS5_etaHEN_v1.1.0.plugin")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")
set(ETAHEN_ROOT "${COMMON_FPS_ETAHEN_SOURCE}/Source Code")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# ------------------------------------------------------------------
# Source-only ShellUI renderer.
# ------------------------------------------------------------------
add_library(common_fps_shellui SHARED)

set_target_properties(common_fps_shellui PROPERTIES
    OUTPUT_NAME "${COMMON_FPS_RENDERER_OUTPUT}"
    PREFIX ""
    SUFFIX ".elf"
    NO_SONAME TRUE
)

target_sources(common_fps_shellui PRIVATE
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/wire.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui_entry.cpp"
)

target_include_directories(common_fps_shellui PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5/shellui_payload"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_shellui PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -fvisibility=hidden
    -march=znver2
    -O2
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

if(COMMON_FPS_SLEEP_RECOVERY)
    target_compile_definitions(common_fps_shellui PRIVATE
        COMMON_FPS_V111_SLEEP_RECOVERY=1
    )
endif()

target_link_options(common_fps_shellui PRIVATE
    -Wl,--gc-sections
    -Wl,-Bsymbolic
    -Wl,-e,elf_main
    -Wl,-soname,libCommonFPS_ShellUI.so
    -Wl,-z,now
    -Wl,--hash-style=sysv
)

target_link_libraries(common_fps_shellui PRIVATE
    kernel_sys
    SceLibcInternal
    SceNet
)

add_custom_command(
    TARGET common_fps_shellui
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:common_fps_shellui>"
            "${ROOT}/dist/${COMMON_FPS_RENDERER_OUTPUT}.elf"
)

# Convert the source-built renderer ELF into a controller-linked byte array.
set(SHELLUI_BLOB_CPP "${GENERATED_DIR}/commonfps_shellui_blob.cpp")
add_custom_command(
    OUTPUT "${SHELLUI_BLOB_CPP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND python3
            "${ROOT}/tools/embed_binary.py"
            "$<TARGET_FILE:common_fps_shellui>"
            "${SHELLUI_BLOB_CPP}"
    DEPENDS
        common_fps_shellui
        "${ROOT}/tools/embed_binary.py"
    VERBATIM
)

# ------------------------------------------------------------------
# Controller/plugin ELF.
# ------------------------------------------------------------------
add_executable(common_fps_controller)

set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "${COMMON_FPS_CONTROLLER_OUTPUT}"
)

target_sources(common_fps_controller PRIVATE
    # v1.1.0 tracked-process lifecycle and FW 9.60 VideoOut sampler.
    "${ROOT}/src/core/fps_sampler.cpp"
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/wire.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"
    "${ROOT}/src/ps5/ps5_platform.cpp"
    "${ROOT}/src/ps5/state_sender.cpp"
    "${ROOT}/src/ps5/shellui_injector.cpp"
    "${ROOT}/src/ps5/stable_shellui_injector.cpp"
    "${ROOT}/src/ps5/remote_call.cpp"
    "${ROOT}/src/ps5/stable_elfldr_bridge.c"
    "${COMMON_FPS_SHSRV_SOURCE}/pt.c"
    "${ROOT}/src/ps5/stable_sampler/process_sysctl.cpp"
    "${ROOT}/src/ps5/stable_sampler/proc_rw_v960.cpp"
    "${ROOT}/src/ps5/stable_sampler/videoout_module.cpp"
    "${SHELLUI_BLOB_CPP}"
)

target_compile_definitions(common_fps_controller PRIVATE
    $<$<BOOL:${COMMON_FPS_SLEEP_RECOVERY}>:COMMON_FPS_V111_SLEEP_RECOVERY=1>
    $<$<NOT:$<BOOL:${COMMON_FPS_SLEEP_RECOVERY}>>:COMMON_FPS_V110_STABLE=1>
    COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG=1
    COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY=1
)

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_ROOT}/fps_elf/include"
    "${COMMON_FPS_SHSRV_SOURCE}"
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
    SceSystemService
    SceNet
    SceSysmodule
    SceUserService
    SceNetCtl
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
            "${ROOT}/dist/${COMMON_FPS_CONTROLLER_OUTPUT}"
    COMMAND python3
            "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/${COMMON_FPS_CONTROLLER_OUTPUT}"
            "${ROOT}/dist/${COMMON_FPS_PLUGIN_OUTPUT}"
            --title-id CFPS00050
            --version 1.39
)
