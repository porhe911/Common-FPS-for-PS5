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
# SR1 - recovered v0.28b backend lifecycle/source probe.
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
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)

target_link_options(common_fps_v028b_sr1 PRIVATE -Wl,--gc-sections)

add_custom_command(
    TARGET common_fps_v028b_sr1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr1"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr1>"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend.elf"
            "${ROOT}/dist/v028b-sr1/Common_FPS_SR1_v028b_Backend_etaHEN.plugin"
            --title-id CFPS00914 --version 9.14
)

# ------------------------------------------------------------------
# SR2 - SR1 lifecycle + one short self debugger-auth window only while
# resolving libSceVideoOut/counter. Auth is restored before periodic reads.
# No ptrace, no MDBG, no renderer/ShellUI injection.
# ------------------------------------------------------------------
add_executable(common_fps_v028b_sr2
    "${ROOT}/src/ps5/legacy_v028b/sr2_backend_auth_probe.cpp"
)

set_target_properties(common_fps_v028b_sr2 PROPERTIES
    OUTPUT_NAME "Common_FPS_SR2_v028b_Auth_DMAP.elf"
)

target_link_libraries(common_fps_v028b_sr2 PRIVATE
    common_fps_v028b_backend
    kernel_sys
    SceLibcInternal
)

target_compile_options(common_fps_v028b_sr2 PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)

target_link_options(common_fps_v028b_sr2 PRIVATE -Wl,--gc-sections)

add_custom_command(
    TARGET common_fps_v028b_sr2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr2"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr2>"
            "${ROOT}/dist/v028b-sr2/Common_FPS_SR2_v028b_Auth_DMAP.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr2/Common_FPS_SR2_v028b_Auth_DMAP.elf"
            "${ROOT}/dist/v028b-sr2/Common_FPS_SR2_v028b_Auth_DMAP_etaHEN.plugin"
            --title-id CFPS00915 --version 9.15
)

# ------------------------------------------------------------------
# SR4 - hardware-proven SR2/SR3 lifecycle/auth timing plus exact v5 module
# syscalls. FW 9.60 returns rc=12 for the zero-capacity size query while still
# supplying a valid count; SR4 accepts that expected result, performs the fill,
# restores Auth, then uses the recovered DMAP reader for table/root/counter.
# ------------------------------------------------------------------
add_executable(common_fps_v028b_sr4
    "${ROOT}/src/ps5/legacy_v028b/sr3_v5_modulelist_dmap_probe.cpp"
)

set_target_properties(common_fps_v028b_sr4 PROPERTIES
    OUTPUT_NAME "Common_FPS_SR4_v028b_V5_RC12_DMAP.elf"
)

target_link_libraries(common_fps_v028b_sr4 PRIVATE
    common_fps_v028b_backend
    kernel_sys
    SceLibcInternal
)

target_compile_options(common_fps_v028b_sr4 PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)

target_link_options(common_fps_v028b_sr4 PRIVATE -Wl,--gc-sections)

add_custom_command(
    TARGET common_fps_v028b_sr4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr4"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr4>"
            "${ROOT}/dist/v028b-sr4/Common_FPS_SR4_v028b_V5_RC12_DMAP.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr4/Common_FPS_SR4_v028b_V5_RC12_DMAP.elf"
            "${ROOT}/dist/v028b-sr4/Common_FPS_SR4_v028b_V5_RC12_DMAP_etaHEN.plugin"
            --title-id CFPS00917 --version 9.17
)

# ------------------------------------------------------------------
# SR5 - keep the hardware-proven SR4 lifecycle/module/auth path, then trace
# every stage of the exact-reference FW 9.60 DMAP translation recovered from
# the stable v0.28b ELF.  No ptrace/MDBG/renderer/injection.
# ------------------------------------------------------------------
add_executable(common_fps_v028b_sr5
    "${ROOT}/src/ps5/legacy_v028b/sr5_dmap_stage_trace.cpp"
)

set_target_properties(common_fps_v028b_sr5 PROPERTIES
    OUTPUT_NAME "Common_FPS_SR5_v028b_DMAP_Stage_Trace.elf"
)

target_link_libraries(common_fps_v028b_sr5 PRIVATE
    common_fps_v028b_backend
    kernel_sys
    SceLibcInternal
)

target_compile_options(common_fps_v028b_sr5 PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)

target_link_options(common_fps_v028b_sr5 PRIVATE -Wl,--gc-sections)

add_custom_command(
    TARGET common_fps_v028b_sr5 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr5"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr5>"
            "${ROOT}/dist/v028b-sr5/Common_FPS_SR5_v028b_DMAP_Stage_Trace.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr5/Common_FPS_SR5_v028b_DMAP_Stage_Trace.elf"
            "${ROOT}/dist/v028b-sr5/Common_FPS_SR5_v028b_DMAP_Stage_Trace_etaHEN.plugin"
            --title-id CFPS00918 --version 9.18
)
