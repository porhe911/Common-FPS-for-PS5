/*
 * Common FPS v0.28b stable-source rebuild - SR1 backend parity probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Hardware probe purpose:
 *   Verify the recovered v0.28b process discovery + DMAP page-table reader +
 *   VideoOut counter path without renderer/ShellUI/injection.
 *
 * Explicitly absent:
 *   ptrace, shsrv, MDBG, debugger-auth switching, FpsSampler, renderer,
 *   SceShellUI injection, game-memory writes.
 */

#include "process_sysctl.hpp"
#include "videoout_counter.hpp"

#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <unistd.h>

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_SR1_v028b_backend.log";
constexpr int kRuntimeSeconds = 300;
constexpr std::uint32_t kMaxTenthsFps = 3000U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;

void log_line(const char* text) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, pid_t pid) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s%d\n", prefix, static_cast<int>(pid));
    std::fclose(f);
}

void log_counter(pid_t pid, std::uintptr_t address) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "SR1 COUNTER READY pid=%d address=0x%llx\n",
                 static_cast<int>(pid),
                 static_cast<unsigned long long>(address));
    std::fclose(f);
}

void log_fps(pid_t pid, std::uint32_t tenths) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "SR1 FPS pid=%d value=%u.%u\n",
                 static_cast<int>(pid), tenths / 10U, tenths % 10U);
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

int run_probe() noexcept {
    using namespace common_fps::legacy_v028b;

    log_pid("SR1 CHILD pid=", getpid());
    log_line("SR1 START recovered v0.28b backend parity probe");
    log_line("SR1 NO ptrace / NO MDBG / NO auth switch / NO renderer / NO ShellUI inject");
    log_line("SR1 path: sysctl eboot.bin -> kernel dynlib VideoOut -> DMAP proc_read -> counter");

    pid_t active_pid = -1;
    std::uintptr_t counter_address = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool have_baseline = false;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        const pid_t current_pid = find_game_pid_sysctl();

        if (current_pid != active_pid) {
            active_pid = current_pid;
            counter_address = 0;
            previous_counter = 0;
            previous_time = {};
            have_baseline = false;
            log_pid("SR1 CHANGE eboot.bin pid=", active_pid);
        }

        if (active_pid <= 0) {
            sleep(1);
            continue;
        }

        if (counter_address == 0) {
            const auto resolved = resolve_videoout_counter(active_pid);
            if (!resolved) {
                log_line("SR1 COUNTER resolve failed; retry next second");
                sleep(1);
                continue;
            }
            counter_address = *resolved;
            log_counter(active_pid, counter_address);
        }

        std::uint32_t current_counter = 0;
        if (!read_videoout_counter(active_pid, counter_address, current_counter)) {
            log_line("SR1 COUNTER read failed; drop attachment and retry discovery");
            counter_address = 0;
            have_baseline = false;
            sleep(1);
            continue;
        }

        timeval now{};
        gettimeofday(&now, nullptr);

        if (!have_baseline) {
            previous_counter = current_counter;
            previous_time = now;
            have_baseline = true;
            log_line("SR1 BASELINE captured");
            sleep(1);
            continue;
        }

        const std::int64_t elapsed = elapsed_us(previous_time, now);
        if (elapsed > 0) {
            const std::uint32_t delta =
                static_cast<std::uint32_t>(current_counter - previous_counter);
            const std::uint64_t scaled =
                static_cast<std::uint64_t>(delta) * kTenthsScale;
            const std::uint32_t tenths = static_cast<std::uint32_t>(
                scaled / static_cast<std::uint64_t>(elapsed));

            if (tenths <= kMaxTenthsFps)
                log_fps(active_pid, tenths);
            else
                log_line("SR1 FPS sanity reject >300.0");
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    log_line("SR1 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR1 PARENT start pid=", getpid());

    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR1 PARENT forked child=", child);
        log_line("SR1 PARENT RETURN 0");
        return 0;
    }

    if (child < 0)
        log_line("SR1 fork failed; follow stable v0.28b fallback and run in current process");

    return run_probe();
}
