cmake_minimum_required(VERSION 3.20)
project(CommonFPS_SR9D C CXX)

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

set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SDK "$ENV{PS5_PAYLOAD_SDK}")
set(ETAHEN_ROOT "${COMMON_FPS_ETAHEN_SOURCE}/Source Code")
set(NINES "${ETAHEN_ROOT}/libNineS")

add_executable(common_fps_sr9d_receiver
    "${ROOT}/src/ps5/legacy_v028b/sr9d_dispatcher_signature_receiver.c"
)
set_target_properties(common_fps_sr9d_receiver PROPERTIES
    OUTPUT_NAME "Common_FPS_SR9D_Dispatcher_Signature_Receiver.elf"
)
target_include_directories(common_fps_sr9d_receiver PRIVATE "${SDK}" "${SDK}/include")
target_compile_options(common_fps_sr9d_receiver PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)
target_link_options(common_fps_sr9d_receiver PRIVATE -Wl,--gc-sections)
target_link_libraries(common_fps_sr9d_receiver PRIVATE kernel_sys SceLibcInternal SceNet)

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(RECEIVER_BLOB_CPP "${GENERATED_DIR}/sr9d_receiver_blob.cpp")
add_custom_command(
    OUTPUT "${RECEIVER_BLOB_CPP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
    COMMAND python3 "${ROOT}/tools/embed_binary_cpp.py"
            "$<TARGET_FILE:common_fps_sr9d_receiver>"
            "${RECEIVER_BLOB_CPP}"
            commonfps_sr9d_receiver_elf
    DEPENDS common_fps_sr9d_receiver "${ROOT}/tools/embed_binary_cpp.py"
    VERBATIM
)

add_library(sr9d_process STATIC "${ROOT}/src/ps5/legacy_v028b/process_sysctl.cpp")
target_include_directories(sr9d_process PUBLIC
    "${ROOT}/src/ps5/legacy_v028b" "${SDK}" "${SDK}/include"
)
target_compile_options(sr9d_process PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)

add_library(sr9d_bootstrap STATIC
    "${ROOT}/src/ps5/legacy_v028b/stable_injector.cpp"
    "${ROOT}/src/ps5/legacy_v028b/pt_call_breakpoint.cpp"
    "${ROOT}/src/ps5/legacy_v028b/libnines_elfldr_trace_bridge.c"
    "${NINES}/src/pt.c"
)
target_include_directories(sr9d_bootstrap PUBLIC
    "${ROOT}/src/ps5/legacy_v028b"
    "${NINES}/include" "${NINES}/src"
    "${ETAHEN_ROOT}/include"
    "${SDK}" "${SDK}/include"
)
target_compile_options(sr9d_bootstrap PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -march=znver2 -O2
    -Wall -Wextra -ffunction-sections -fdata-sections
)

add_executable(common_fps_v028b_sr9d
    "${ROOT}/src/ps5/legacy_v028b/sr9d_dispatcher_signature_bootstrap_probe.cpp"
    "${RECEIVER_BLOB_CPP}"
)
set_target_properties(common_fps_v028b_sr9d PROPERTIES
    OUTPUT_NAME "Common_FPS_SR9D_v028b_Dispatcher_Signature.elf"
)
target_include_directories(common_fps_v028b_sr9d PRIVATE
    "${ROOT}/src/ps5/legacy_v028b" "${NINES}/include" "${SDK}" "${SDK}/include"
)
target_compile_options(common_fps_v028b_sr9d PRIVATE
    --target=x86_64-sie-ps5 -DPS5 -fPIC -fPIE -march=znver2 -O2
    -Wall -Wextra -Werror -ffunction-sections -fdata-sections
)
target_link_options(common_fps_v028b_sr9d PRIVATE -Wl,--gc-sections)
target_link_libraries(common_fps_v028b_sr9d PRIVATE
    sr9d_process sr9d_bootstrap kernel_sys SceLibcInternal SceNet
)

add_custom_command(
    TARGET common_fps_v028b_sr9d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ROOT}/dist/v028b-sr9d"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_sr9d_receiver>"
            "${ROOT}/dist/v028b-sr9d/Common_FPS_SR9D_Dispatcher_Signature_Receiver.elf"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:common_fps_v028b_sr9d>"
            "${ROOT}/dist/v028b-sr9d/Common_FPS_SR9D_v028b_Dispatcher_Signature.elf"
    COMMAND python3 "${ROOT}/tools/make_etahen_plugin.py"
            "${ROOT}/dist/v028b-sr9d/Common_FPS_SR9D_v028b_Dispatcher_Signature.elf"
            "${ROOT}/dist/v028b-sr9d/Common_FPS_SR9D_v028b_Dispatcher_Signature_etaHEN.plugin"
            --title-id CFPS00926 --version 9.26
)
