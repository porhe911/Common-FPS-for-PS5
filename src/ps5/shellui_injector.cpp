/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "shellui_injector.hpp"
#include "shellui_blob.hpp"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

extern "C" {
#include "elfldr.h"
#include "proc.h"
}

namespace common_fps::ps5 {
namespace {

constexpr const char* kMarker = "/system_tmp/commonfps_shellui.pid";
constexpr const char* kLog = "/data/CommonFPS_v110_source.log";

void log_line(const char* fmt, ...) {
    FILE* fp = std::fopen(kLog, "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}

bool marker_matches(pid_t pid) {
    FILE* fp = std::fopen(kMarker, "r");
    if (!fp)
        return false;

    int marked_pid = -1;
    const int rc = std::fscanf(fp, "%d", &marked_pid);
    std::fclose(fp);
    return rc == 1 && marked_pid == pid;
}

} // namespace

bool ensure_shellui_renderer() {
    static pid_t attempted_pid = -1;
    static bool attempted_result = false;

    struct proc* process = find_proc_by_name("SceShellUI");
    if (!process) {
        log_line("ShellUI process not found");
        return false;
    }

    const pid_t pid = process->pid;
    std::free(process);

    if (marker_matches(pid))
        return true;

    /*
     * Never inject the same payload repeatedly into one ShellUI process.
     * If initialization failed, the diagnostic ShellUI log is more useful
     * than stacking another resident copy on top of it.
     */
    if (attempted_pid == pid)
        return attempted_result;

    attempted_pid = pid;
    attempted_result = false;

    log_line(
        "ShellUI inject start pid=%d payload_size=%zu",
        pid,
        commonfps_shellui_elf_size);

    if (commonfps_shellui_elf_size == 0 ||
        elfldr_exec(
            -1,
            -1,
            -1,
            pid,
            const_cast<std::uint8_t*>(commonfps_shellui_elf)) != 0) {
        log_line("ShellUI elfldr_exec failed pid=%d", pid);
        return false;
    }

    /* Give the injected payload time to resolve Mono/PUI and bind UDP. */
    for (int i = 0; i < 150; ++i) {
        if (marker_matches(pid)) {
            attempted_result = true;
            log_line("ShellUI renderer online pid=%d", pid);
            return true;
        }
        usleep(20000);
    }

    log_line("ShellUI marker timeout pid=%d", pid);
    return false;
}

} // namespace common_fps::ps5
