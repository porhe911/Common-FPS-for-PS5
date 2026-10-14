/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "common_fps/shellui_stage.hpp"
#include "common_fps/session_log.hpp"

#include <cstdio>
#include <unistd.h>

namespace {

constexpr const char* kMarker = "/system_tmp/commonfps_shellui.pid";

void write_online_marker() {
    FILE* fp = std::fopen(kMarker, "w");
    if (!fp)
        return;
    std::fprintf(fp, "%d\n", getpid());
    std::fclose(fp);
}

[[noreturn]] void park_renderer_failure(const char* stage) {
    common_fps::log_message(common_fps::kRendererLogPath,
        "renderer parked stage=%s injected_thread_return=disabled", stage);

    /*
     * Returning an injected ShellUI ELF thread can destabilize the console.
     * A compatibility or socket failure therefore remains inert and resident
     * instead of returning through the loader. The v1.2.3 renderer preserves that proven
     * lifecycle and patches a native hook only while ShellUI is stopped.
     */
    for (;;)
        usleep(1000000);
}

} // namespace

namespace common_fps::ps5::shellui {

void record_stage(const char* stage) noexcept {
    if (FILE* fp = std::fopen(kShellUiStagePath, "w")) {
        std::fprintf(fp, "%d %.47s\n", getpid(), stage);
        std::fclose(fp);
    }
}

} // namespace common_fps::ps5::shellui

#if defined(COMMON_FPS_TEST23_PARK_BEFORE_RUNTIME)
extern "C" {
/*
 * Keep the complete renderer reachable in this diagnostic ELF so its image,
 * imports and relocations remain representative of TEST22. The exported
 * volatile gate is initialized to zero and is never changed by the controller;
 * therefore hardware executes only the pre-runtime park branch. Keeping a
 * runtime-selectable branch prevents --gc-sections from reducing TEST23 to a
 * different minimal payload.
 */
__attribute__((used, visibility("default")))
volatile int common_fps_test23_runtime_gate = 0;
}
#endif

extern "C" __attribute__((noreturn, visibility("default")))
void* elf_main(void* payload_args) {
    using namespace common_fps::ps5::shellui;

    /* A shared renderer does not start the PS5 payload CRT in SceShellUI. */
    (void)payload_args;

    record_stage("entry");

    common_fps::reset_session_log(common_fps::kRendererLogPath);
    common_fps::log_message(common_fps::kRendererLogPath,
        "Common FPS for PS5 v1.2.3 RC4 renderer log_limit=65536");

#if defined(COMMON_FPS_TEST23_PARK_BEFORE_RUNTIME)
    if (common_fps_test23_runtime_gate == 0) {
        /*
         * Prove that elf_main was entered, then remain resident without any
         * Mono/PUI lookup, Application.Update patch, GC handle, socket, bind,
         * recv or widget-tree operation. Returning this injected thread is
         * permanently forbidden by the TEST8 KP result.
         */
        write_online_marker();
        park_renderer_failure("test23_pre_runtime");
    }
#endif

    bool runtime_ready = false;
    for (int attempt = 0; attempt < 60; ++attempt) {
        if (initialize_runtime()) {
            runtime_ready = true;
            break;
        }
        usleep(1000000);
    }

    if (!runtime_ready)
        park_renderer_failure("runtime");

    record_stage("runtime_ready");

    if (!initialize_receiver())
        park_renderer_failure("receiver");

    record_stage("receiver_ready");

    write_online_marker();
    run_receiver_loop();

    __builtin_unreachable();
}
