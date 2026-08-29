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
# Source-built ShellUI renderer.
#
# This target is an INTERNAL build artifact. It is converted to a byte array
# and linked into the public controller ELF/plugin below, so end users do not
# need a separate companion file on disk.
# ------------------------------------------------------------------
add_executable(common_fps_shellui)
set_target_properties(common_fps_shellui PROPERTIES
    OUTPUT_NAME "Common_FPS_ShellUI_v1.1.0.elf")

target_sources(common_fps_shellui PRIVATE
    "${ROOT}/src/core/layout.cpp"
    "${ROOT}/src/core/wire.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui.cpp"
    "${ROOT}/src/ps5/shellui_payload/commonfps_shellui_entry.cpp"
    "${ETAHEN_SHELLUI}/src/MonoUtils.cpp"
    "${ETAHEN_SHELLUI}/src/external_symbols.cpp"
    "${ETAHEN_SHELLUI}/src/Detour.cpp"
    "${ETAHEN_SHELLUI}/src/hde64.cpp"
    "${ETAHEN_SHELLUI}/src/proc.cpp"
    "${ETAHEN_SHELLUI}/src/ucred.cpp"
)

target_include_directories(common_fps_shellui PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5/shellui_payload"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_SHELLUI}/include"
    "${ETAHEN_SHELLUI}/lib"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_shellui PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -ffunction-sections -fdata-sections -Wno-c++11-narrowing)
target_link_options(common_fps_shellui PRIVATE -Wl,--gc-sections)
target_link_libraries(common_fps_shellui PRIVATE
    kernel_sys SceNet SceSysmodule SceSystemService SceLibcInternal)

# Convert the just-built ShellUI ELF into generated C++ bytes. This creates a
# strict build dependency: public controller -> embedded blob -> ShellUI ELF.
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(EMBEDDED_RENDERER_CPP "${GENERATED_DIR}/commonfps_shellui_blob.cpp")

add_custom_command(
    OUTPUT "${EMBEDDED_RENDERER_CPP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND python3 "${ROOT}/tools/embed_binary_cpp.py"
            "$<TARGET_FILE:common_fps_shellui>"
            "${EMBEDDED_RENDERER_CPP}"
    DEPENDS common_fps_shellui "${ROOT}/tools/embed_binary_cpp.py"
    COMMENT "Embedding source-built ShellUI renderer into Common FPS controller"
    VERBATIM
)
set_source_files_properties("${EMBEDDED_RENDERER_CPP}" PROPERTIES GENERATED TRUE)
add_custom_target(common_fps_embed_renderer DEPENDS "${EMBEDDED_RENDERER_CPP}")

# ------------------------------------------------------------------
# Public standalone ELF / etaHEN plugin controller.
# Both output forms contain the same embedded ShellUI renderer.
# ------------------------------------------------------------------
add_executable(common_fps_controller)
set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_PS5_v1.1.0.elf")

add_dependencies(common_fps_controller common_fps_embed_renderer)

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
    "${EMBEDDED_RENDERER_CPP}"
    "${ETAHEN_FPS}/src/proc.cpp"
    "${ETAHEN_SHELLUI}/src/ucred.cpp"
    "${COMMON_FPS_SHSRV_SOURCE}/pt.c"
    "${COMMON_FPS_SHSRV_SOURCE}/elfldr.c"
)

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
    "${ETAHEN_SHELLUI}/include"
    "${COMMON_FPS_SHSRV_SOURCE}"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_controller PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -ffunction-sections -fdata-sections)
target_link_options(common_fps_controller PRIVATE -Wl,--gc-sections)
target_link_libraries(common_fps_controller PRIVATE
    kernel_sys SceLibcInternal SceSystemService SceNet SceSysmodule
    SceUserService SceNetCtl)

# The public distribution contains exactly two launch forms:
#   1) standalone ELF
#   2) etaHEN plugin wrapper around that same ELF
add_custom_command(TARGET common_fps_controller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_controller>"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_PS5_v1.1.0.elf"
            "${ROOT}/dist/Common_FPS_PS5_etaHEN_v1.1.0.plugin"
            --title-id CFPS00021 --version 1.10)
