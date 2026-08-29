/*
 * Common FPS for PS5 - RC6 fork-only resident worker safety probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdio>
#include <unistd.h>

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_RC6_fork_only.log";

void log_line(const char* text) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, pid_t pid) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s%d\n", prefix, static_cast<int>(pid));
    std::fclose(f);
}

int run_child() {
    log_pid("RC6 CHILD pid=", getpid());
    log_line("RC6 CHILD resident; NO sysctl / NO kernel / NO ptrace / NO auth / NO inject / NO FPS");

    // Deliberately short-lived diagnostic resident worker. 12 heartbeats x 5 s
    // gives one minute of runtime, then the child exits cleanly on its own.
    for (int i = 1; i <= 12; ++i) {
        sleep(5);
        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "RC6 ALIVE %d/12\n", i);
            std::fclose(f);
        }
    }

    log_line("RC6 CHILD DONE clean return");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("RC6 PARENT start pid=", getpid());

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC6 fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC6 PARENT forked child=", pid);
        log_line("RC6 PARENT RETURN 0");
        return 0;
    }

    return run_child();
}
