/*
 * Common FPS v0.28b stable-source rebuild - SR6 production backend probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unlike SR5, this file contains no private DMAP/module-discovery copy.  It
 * validates the reusable production modules that will be used by the final
 * producer: process_sysctl.cpp + proc_rw_v960.cpp + videoout_counter.cpp.
 */

#include "process_sysctl.hpp"
#include "videoout_counter.hpp"

#include <cstdint>
#include <cstdio>
#include <sys/time.h>
#include <unistd.h>

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_SR6_production_backend.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint32_t kMaxTenthsFps = 1300U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;
constexpr unsigned kWarmupDeltasToDiscard = 1U;

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

void log_counter(pid_t pid, std::uintptr_t address) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR6 COUNTER READY pid=%d address=0x%llx\n",
                 static_cast<int>(pid),
                 static_cast<unsigned long long>(address));
    std::fclose(f);
}

void log_fps(pid_t pid, std::uint32_t tenths) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR6 FPS pid=%d value=%u.%u\n",
                 static_cast<int>(pid), tenths / 10U, tenths % 10U);
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

int run_probe() noexcept {
    using common_fps::legacy_v028b::find_game_pid_sysctl;
    using common_fps::legacy_v028b::read_videoout_counter;
    using common_fps::legacy_v028b::resolve_videoout_counter;

    log_pid("SR6 CHILD pid=", getpid());
    log_line("SR6 START production backend validation");
    log_line("SR6 USES shared proc_rw_v960 + shared videoout_counter");
    log_line("SR6 NO ptrace / NO MDBG / NO renderer / NO ShellUI inject");
    log_line("SR6 warmup=baseline+discard-one-delta sanity_cap=130.0");

    pid_t active_pid = -1;
    std::uintptr_t counter_address = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool have_baseline = false;
    unsigned warmup_deltas = kWarmupDeltasToDiscard;
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
            warmup_deltas = kWarmupDeltasToDiscard;
            seconds_on_pid = 0;
            retry_countdown = 0;
            log_pid("SR6 CHANGE eboot.bin pid=", active_pid);
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

            log_line("SR6 DISCOVERY TRY shared production backend");
            const auto resolved = resolve_videoout_counter(active_pid);
            if (!resolved) {
                log_line("SR6 DISCOVERY not ready; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }

            counter_address = *resolved;
            have_baseline = false;
            warmup_deltas = kWarmupDeltasToDiscard;
            log_counter(active_pid, counter_address);
        }

        std::uint32_t current_counter = 0;
        if (!read_videoout_counter(active_pid, counter_address, current_counter)) {
            log_line("SR6 COUNTER read failed; drop and rediscover");
            counter_address = 0;
            have_baseline = false;
            warmup_deltas = kWarmupDeltasToDiscard;
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
            log_line("SR6 WARMUP baseline captured");
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

            if (warmup_deltas != 0) {
                --warmup_deltas;
                log_line("SR6 WARMUP first delta discarded");
            } else if (tenths > kMaxTenthsFps) {
                log_line("SR6 FPS sanity reject >130.0; keep counter attached");
            } else {
                log_fps(active_pid, tenths);
            }
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    log_line("SR6 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR6 PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR6 PARENT forked child=", child);
        log_line("SR6 PARENT RETURN 0");
        return 0;
    }
    if (child < 0)
        log_line("SR6 fork failed; run in current process");
    return run_probe();
}
