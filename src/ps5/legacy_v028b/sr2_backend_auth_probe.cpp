/*
 * Common FPS v0.28b stable-source rebuild - SR2 auth-gated backend probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SR2 preserves the hardware-stable SR1 lifecycle and changes only module
 * discovery: the payload's own Auth ID is temporarily switched to debugger
 * while resolving libSceVideoOut + the v0.28b counter. The original Auth ID is
 * restored immediately. Periodic FPS reads use only the recovered DMAP reader.
 */

#include "process_sysctl.hpp"
#include "videoout_counter.hpp"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_SR2_v028b_auth_dmap.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint32_t kMaxTenthsFps = 3000U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;
constexpr std::uint64_t kDebuggerAuthId = 0x4800000000000006ULL;
constexpr std::uintptr_t kAuthIdOffset = 0x58ULL;

void log_line(const char* text) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, pid_t pid) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s%d\n", prefix, static_cast<int>(pid));
    std::fclose(f);
}

void log_auth(const char* tag, std::uint64_t value) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR2 %s auth=0x%016llx\n", tag,
                 static_cast<unsigned long long>(value));
    std::fclose(f);
}

void log_counter(pid_t pid, std::uintptr_t address) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR2 COUNTER READY pid=%d address=0x%llx\n",
                 static_cast<int>(pid),
                 static_cast<unsigned long long>(address));
    std::fclose(f);
}

void log_fps(pid_t pid, std::uint32_t tenths) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR2 FPS pid=%d value=%u.%u\n",
                 static_cast<int>(pid), tenths / 10U, tenths % 10U);
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

std::optional<std::uintptr_t> resolve_with_short_self_auth(pid_t game_pid) noexcept {
    using namespace common_fps::legacy_v028b;

    const std::uintptr_t self_ucred = kernel_get_proc_ucred(getpid());
    if (self_ucred == 0) {
        log_line("SR2 AUTH failed: kernel_get_proc_ucred(self)=0");
        return std::nullopt;
    }

    std::uint64_t saved_auth = 0;
    if (kernel_copyout(self_ucred + kAuthIdOffset, &saved_auth, sizeof(saved_auth)) != 0) {
        log_line("SR2 AUTH failed: read current Auth ID");
        return std::nullopt;
    }
    log_auth("AUTH saved", saved_auth);

    const std::uint64_t debug_auth = kDebuggerAuthId;
    if (kernel_copyin(&debug_auth, self_ucred + kAuthIdOffset, sizeof(debug_auth)) != 0) {
        log_line("SR2 AUTH failed: set debugger Auth ID");
        return std::nullopt;
    }
    log_line("SR2 AUTH debugger active only for VideoOut discovery");

    const auto resolved = resolve_videoout_counter(game_pid);

    if (kernel_copyin(&saved_auth, self_ucred + kAuthIdOffset, sizeof(saved_auth)) != 0) {
        log_line("SR2 AUTH RESTORE FAILED");
        return std::nullopt;
    }
    log_line("SR2 AUTH restored before periodic sampling");

    return resolved;
}

int run_probe() noexcept {
    using namespace common_fps::legacy_v028b;

    log_pid("SR2 CHILD pid=", getpid());
    log_line("SR2 START v0.28b short-auth + DMAP parity probe");
    log_line("SR2 NO ptrace / NO MDBG / NO renderer / NO ShellUI inject");
    log_line("SR2 auth only during VideoOut discovery; periodic reads are DMAP kernel_copyout");

    pid_t active_pid = -1;
    std::uintptr_t counter_address = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool have_baseline = false;
    int seconds_on_pid = 0;
    int retry_countdown = 0;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        const pid_t current_pid = find_game_pid_sysctl();

        if (current_pid != active_pid) {
            active_pid = current_pid;
            counter_address = 0;
            previous_counter = 0;
            previous_time = {};
            have_baseline = false;
            seconds_on_pid = 0;
            retry_countdown = 0;
            log_pid("SR2 CHANGE eboot.bin pid=", active_pid);
        }

        if (active_pid <= 0) {
            sleep(1);
            continue;
        }

        ++seconds_on_pid;

        if (counter_address == 0) {
            if (seconds_on_pid < kDiscoveryDelaySeconds) {
                sleep(1);
                continue;
            }
            if (retry_countdown > 0) {
                --retry_countdown;
                sleep(1);
                continue;
            }

            log_line("SR2 DISCOVERY TRY short self-auth window");
            const auto resolved = resolve_with_short_self_auth(active_pid);
            if (!resolved) {
                log_line("SR2 COUNTER resolve failed; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }
            counter_address = *resolved;
            log_counter(active_pid, counter_address);
        }

        std::uint32_t current_counter = 0;
        if (!read_videoout_counter(active_pid, counter_address, current_counter)) {
            log_line("SR2 COUNTER DMAP read failed; drop counter and rediscover later");
            counter_address = 0;
            have_baseline = false;
            retry_countdown = kDiscoveryRetrySeconds;
            sleep(1);
            continue;
        }

        timeval now{};
        gettimeofday(&now, nullptr);

        if (!have_baseline) {
            previous_counter = current_counter;
            previous_time = now;
            have_baseline = true;
            log_line("SR2 BASELINE captured");
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
                log_line("SR2 FPS sanity reject >300.0");
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    log_line("SR2 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR2 PARENT start pid=", getpid());

    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR2 PARENT forked child=", child);
        log_line("SR2 PARENT RETURN 0");
        return 0;
    }

    if (child < 0)
        log_line("SR2 fork failed; run in current process");

    return run_probe();
}
