cmake_minimum_required(VERSION 3.20)
project(CommonFPS_PS5 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS5_PAYLOAD_SDK})
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set")
endif()

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")
set(ETAHEN_ROOT "${ROOT}/.deps/etahen/Source Code")
set(ETAHEN_FPS "${ETAHEN_ROOT}/fps_elf")
set(ETAHEN_SHELLUI "${ETAHEN_ROOT}/shellui")

if(NOT EXISTS "${ETAHEN_FPS}/src/proc.cpp")
    message(FATAL_ERROR "Pinned etaHEN source missing; run scripts/prepare_ps5_deps.sh")
endif()

# RC13 keeps the RC12 one-time discovery path, but never reuses the process
# that performs privileged inspection. A clean sysctl-only supervisor forks
# one short-lived discovery child per confirmed game PID. The child exits
# immediately after resolving the VideoOut FPS counter and never samples FPS.
add_executable(common_fps_controller
    "${ROOT}/src/core/fps_sampler.cpp"
    "${ROOT}/src/ps5/ps5_platform.cpp"
    "${ROOT}/src/ps5/commonfps_ps5_main.cpp"
    "${ETAHEN_FPS}/src/proc.cpp"
    "${ETAHEN_SHELLUI}/src/ucred.cpp"
)
set_target_properties(common_fps_controller PROPERTIES
    OUTPUT_NAME "Common_FPS_RC13_Fresh_Child_Discovery.elf")

target_include_directories(common_fps_controller PRIVATE
    "${ROOT}/include"
    "${ROOT}/src/ps5"
    "${ETAHEN_ROOT}/include"
    "${ETAHEN_FPS}/include"
    "${ETAHEN_SHELLUI}/include"
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
            "${ROOT}/dist/Common_FPS_RC13_Fresh_Child_Discovery.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/Common_FPS_RC13_Fresh_Child_Discovery.elf"
            "${ROOT}/dist/Common_FPS_RC13_Fresh_Child_Discovery_etaHEN.plugin"
            --title-id CFPS00913 --version 9.13)
