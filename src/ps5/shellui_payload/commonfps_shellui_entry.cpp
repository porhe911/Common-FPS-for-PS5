/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"

#include <cstdio>
#include <unistd.h>

namespace {

constexpr const char* kMarker = "/system_tmp/commonfps_shellui.pid";
constexpr const char* kLog = "/data/CommonFPS_v110_shellui.log";

void write_online_marker() {
    FILE* fp = std::fopen(kMarker, "w");
    if (!fp)
        return;
    std::fprintf(fp, "%d\n", getpid());
    std::fclose(fp);
}

[[noreturn]] void park_renderer_failure(const char* stage) {
    if (FILE* fp = std::fopen(kLog, "a")) {
        std::fprintf(
            fp,
            "renderer parked stage=%s injected_thread_return=disabled\n",
            stage);
        std::fclose(fp);
    }

    /*
     * TEST8 demonstrated that returning an injected ShellUI ELF thread can
     * KP the console. Even a compatibility or socket failure must therefore
     * remain inert and resident instead of returning through the loader.
     * TEST13 retains the TEST12 receive timeout/stale-state transition; it
     * does not reintroduce a remote-thread return path.
     */
    for (;;)
        usleep(1000000);
}

} // namespace

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

    if (FILE* fp = std::fopen(kLog, "w")) {
        std::fputs(
            "Common FPS v1.1.0 PARITY TEST13 target-thread bootstrap stack\n",
            fp);
        std::fclose(fp);
    }

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

    if (!initialize_receiver())
        park_renderer_failure("receiver");

    write_online_marker();
    run_receiver_loop();

    __builtin_unreachable();
}
