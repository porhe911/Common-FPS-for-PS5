# Hardware-proven v0.28b producer reconstruction.
#
# The static backend compiles the recovered FW 9.60 lifecycle + DMAP +
# VideoOut + PHUF producer path. The bootstrap static library is deliberately
# separate so startup injection can be audited without contaminating the FPS
# lifecycle with ptrace/MDBG dependencies.

add_library(common_fps_v028b_backend STATIC
    "${ROOT}/src/ps5/legacy_v028b/process_sysctl.cpp"
    "${ROOT}/src/ps5/legacy_v028b/proc_rw_v960.cpp"
    "${ROOT}/src/ps5/legacy_v028b/videoout_counter.cpp"
    "${ROOT}/src/ps5/legacy_v028b/stable_producer.cpp"
)

set_target_properties(common_fps_v028b_backend PROPERTIES
    OUTPUT_NAME "Common_FPS_v028b_backend_sourceproof"
)

target_include_directories(common_fps_v028b_backend PUBLIC
    "${ROOT}/src/ps5/legacy_v028b"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_v028b_backend PRIVATE
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

# ------------------------------------------------------------------
# v0.28b one-time ShellUI bootstrap reconstruction (compile-only).
#
# This target intentionally contains ptrace/shsrv because the stable payload
# uses ptrace once during renderer startup. It is NOT linked into the periodic
# FPS producer path and is not emitted as a runnable payload by source-proof CI.
# ------------------------------------------------------------------
add_library(common_fps_v028b_bootstrap STATIC
    "${ROOT}/src/ps5/legacy_v028b/stable_injector.cpp"
    "${ROOT}/src/ps5/legacy_v028b/pt_call_breakpoint.cpp"
    "${ROOT}/src/ps5/legacy_v028b/shsrv_elfldr_bridge.c"
    "${COMMON_FPS_SHSRV_SOURCE}/pt.c"
)

set_target_properties(common_fps_v028b_bootstrap PROPERTIES
    OUTPUT_NAME "Common_FPS_v028b_bootstrap_sourceproof"
)

target_include_directories(common_fps_v028b_bootstrap PUBLIC
    "${ROOT}/src/ps5/legacy_v028b"
    "${COMMON_FPS_SHSRV_SOURCE}"
    "${SDK}"
    "${SDK}/include"
)

target_compile_options(common_fps_v028b_bootstrap PRIVATE
    --target=x86_64-sie-ps5
    -DPS5
    -fPIC
    -march=znver2
    -O2
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# ------------------------------------------------------------------
# SR1 - recovered v0.28b backend hardware parity probe.
#
# No renderer, ShellUI injection, shsrv ptrace, MDBG or credential switching.
# main() preserves the proven async fork entry and then exercises only:
# sysctl lifecycle -> kernel dynlib VideoOut -> DMAP proc_read -> counter.
#
# SR1 exists for later controlled hardware parity testing; source-rebuild CI
# does not automatically publish it as the next user test.
# ------------------------------------------------------------------
add_executable(common_fps_v028b_sr1
    "${ROOT}/src/ps5/legacy_v028b/sr1_backend_probe.cpp"
)

set_target_properties(common_fps_v028b_sr1 PROPERTIES
    OUTPUT_NAME "Common_FPS_SR1_v028b_Backend.elf"
)

target_link_libraries(common_fps_v028b_sr1 PRIVATE
    common_fps_v028b_backend
    kernel_sys
    SceLibcInternal
)

target_compile_options(common_fps_v028b_sr1 PRIVATE
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

target_link_options(common_fps_v028b_sr1 PRIVATE
    -Wl,--gc-sections
)

add_custom_command(
    TARGET common_fps_v028b_sr1
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr1"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:common_fps_v028b_sr1>"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend.elf"
    COMMAND python3
            "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend.elf"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend_etaHEN.plugin"
            --title-id CFPS0SR01
            --version 0.281
)
