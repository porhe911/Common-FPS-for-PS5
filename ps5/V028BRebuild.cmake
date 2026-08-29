# Hardware-proven v0.28b producer reconstruction.
#
# This target is intentionally a static library only. It compiles the recovered
# FW 9.60 lifecycle + DMAP + VideoOut + PHUF producer path without producing a
# payload that could accidentally be hardware-tested before renderer/bootstrap
# parity.

add_library(common_fps_v028b_backend STATIC
    "${ROOT}/src/ps5/legacy_v028b/process_sysctl.cpp"
    "${ROOT}/src/ps5/legacy_v028b/proc_rw_v960.cpp"
    "${ROOT}/src/ps5/legacy_v028b/videoout_counter.cpp"
    "${ROOT}/src/ps5/legacy_v028b/stable_producer.cpp"
)

set_target_properties(common_fps_v028b_backend PROPERTIES
    OUTPUT_NAME "Common_FPS_v028b_backend_sourceproof"
)

target_include_directories(common_fps_v028b_backend PRIVATE
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
