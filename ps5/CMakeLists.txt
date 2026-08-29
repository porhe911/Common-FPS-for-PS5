cmake_minimum_required(VERSION 3.20)
project(CommonFPS_PS5 C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")
set(ETAHEN_ROOT "${ROOT}/.deps/etahen/Source Code")
set(ETAHEN_FPS "${ETAHEN_ROOT}/fps_elf")
set(ETAHEN_SHELLUI "${ETAHEN_ROOT}/shellui")
set(SHSRV_ROOT "${ROOT}/.deps/shsrv")

if(NOT EXISTS "${ETAHEN_FPS}/src/proc.cpp")
    message(FATAL_ERROR "Pinned etaHEN source missing; run scripts/prepare_ps5_deps.sh")
endif()
if(NOT EXISTS "${SHSRV_ROOT}/pt.c")
    message(FATAL_ERROR "Pinned shsrv source missing; run scripts/prepare_ps5_deps.sh")
endif()

# RC9 intentionally restores ONLY the v11 FPS backend on top of RC8-proven
# sparse process discovery. No renderer, ShellUI payload, Mono, detour,
# elfldr, health UDP, state sender, or overlay code is linked.
add_executable(common_fps_controller
    "${ROOT}/src/core/fps_sampler.cpp"
    "${ROOT}/src/ps5/ps5_platform.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"
    "${ETAHEN_FPS}/src/proc.cpp"
    "${ETAHEN_SHELLUI}/src/ucred.cpp"
    "${SHSRV_ROOT}/pt.c"
)
set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_RC9_FPS_Backend.elf")

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
    "${ETAHEN_SHELLUI}/include"
    "${SHSRV_ROOT}"
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

add_custom_command(TARGET common_fps_controller POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_controller>"
            "${ROOT}/dist/Common_FPS_RC9_FPS_Backend.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_RC9_FPS_Backend.elf"
            "${ROOT}/dist/Common_FPS_RC9_FPS_Backend_etaHEN.plugin"
            --title-id CFPS00909 --version 9.09)
