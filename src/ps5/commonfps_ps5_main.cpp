/*
 * Common FPS for PS5 - RC12 discovery-only probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/fps_sampler.hpp"
#include "ps5_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_RC12_discovery_only.log";
constexpr int kRuntimeSeconds = 240;
constexpr int kSnapshotPeriodSeconds = 5;
constexpr int kSnapshotCount = kRuntimeSeconds / kSnapshotPeriodSeconds;
constexpr int kStableSnapshotsBeforeAttach = 2;

void log_line(const char* text) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, int pid) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s%d\n", prefix, pid);
    std::fclose(f);
}

bool bounded_name_equals(const std::uint8_t* record, std::size_t record_size, const char* wanted) {
    constexpr std::size_t kNameOffset = 447U;
    if (record_size <= kNameOffset) return false;
    const std::size_t wanted_len = std::strlen(wanted);
    const std::size_t available = record_size - kNameOffset;
    if (available < wanted_len + 1U) return false;
    const auto* name = record + kNameOffset;
    return std::memcmp(name, wanted, wanted_len) == 0 && name[wanted_len] == 0;
}

struct SnapshotResult {
    int shellui_pid = -1;
    int game_pid = -1;
    std::size_t records = 0;
    std::size_t filled = 0;
    int rc = -1;
};

SnapshotResult take_snapshot() {
    SnapshotResult out{};
    int mib[4] = {1, 14, 8, 0};
    std::size_t required = 0;
    if (sysctl(mib, 4, nullptr, &required, nullptr, 0) != 0 || required == 0) return out;
    const std::size_t capacity = required + 65536U;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!buffer) return out;
    std::size_t filled = capacity;
    if (sysctl(mib, 4, buffer, &filled, nullptr, 0) != 0 || filled > capacity) {
        std::free(buffer);
        return out;
    }
    std::uint8_t* ptr = buffer;
    std::uint8_t* end = buffer + filled;
    while (ptr < end) {
        if (static_cast<std::size_t>(end - ptr) < sizeof(int)) break;
        int record_size_signed = 0;
        std::memcpy(&record_size_signed, ptr, sizeof(record_size_signed));
        if (record_size_signed <= 0) break;
        const std::size_t record_size = static_cast<std::size_t>(record_size_signed);
        if (record_size > static_cast<std::size_t>(end - ptr)) break;
        ++out.records;
        if (record_size >= 76U) {
            int pid = -1;
            std::memcpy(&pid, ptr + 72U, sizeof(pid));
            if (out.shellui_pid < 0 && bounded_name_equals(ptr, record_size, "SceShellUI")) out.shellui_pid = pid;
            if (out.game_pid < 0 && bounded_name_equals(ptr, record_size, "eboot.bin")) out.game_pid = pid;
        }
        ptr += record_size;
    }
    out.filled = filled;
    out.rc = 0;
    std::free(buffer);
    return out;
}

void log_snapshot(int index, const SnapshotResult& s, int stable_count, bool attached) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f,
        "RC12 SNAPSHOT %d/%d rc=%d records=%zu filled=%zu shellui=%d game=%d stable=%d attached=%d\n",
        index, kSnapshotCount, s.rc, s.records, s.filled, s.shellui_pid, s.game_pid,
        stable_count, attached ? 1 : 0);
    std::fclose(f);
}

void log_attach_ok(int pid, std::uintptr_t counter) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "RC12 DISCOVERY OK pid=%d counter=0x%llx; NO FURTHER MEMORY READS\n",
        pid, static_cast<unsigned long long>(counter));
    std::fclose(f);
}

int run_child() {
    log_pid("RC12 CHILD pid=", static_cast<int>(getpid()));
    log_line("RC12 discovery-only; NO renderer / NO ShellUI inject / NO FPS sampling");
    log_line("RC12 sysctl lifecycle every 5s; new PID must survive two snapshots");
    log_line("RC12 FpsSampler.attach runs ONCE per game only to discover VideoOut/counter");
    log_line("RC12 after DISCOVERY OK performs ZERO remote memory reads until PID changes");

    common_fps::ps5::Ps5Platform platform;
    common_fps::FpsSampler sampler(platform);
    int observed_game = -2;
    int stable_count = 0;

    for (int index = 1; index <= kSnapshotCount; ++index) {
        const SnapshotResult s = take_snapshot();
        if (s.rc != 0) {
            log_snapshot(index, s, stable_count, sampler.attached());
            sleep(kSnapshotPeriodSeconds);
            continue;
        }

        if (s.game_pid != observed_game) {
            observed_game = s.game_pid;
            stable_count = 1;
            sampler.reset();
            log_pid("RC12 CHANGE eboot.bin pid=", observed_game);
        } else if (stable_count < 1000) {
            ++stable_count;
        }

        log_snapshot(index, s, stable_count, sampler.attached());

        if (observed_game > 0 && stable_count >= kStableSnapshotsBeforeAttach && !sampler.attached()) {
            log_pid("RC12 DISCOVERY TRY confirmed pid=", observed_game);
            if (sampler.attach(observed_game)) log_attach_ok(observed_game, sampler.counter_address());
            else log_line("RC12 DISCOVERY RETRY on next confirmed snapshot");
        }

        sleep(kSnapshotPeriodSeconds);
    }

    sampler.reset();
    log_line("RC12 CHILD DONE clean return");
    return 0;
}
}

extern "C" int main() {
    log_pid("RC12 PARENT start pid=", static_cast<int>(getpid()));
    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC12 fork FAIL");
        return 1;
    }
    if (pid > 0) {
        log_pid("RC12 PARENT forked child=", static_cast<int>(pid));
        log_line("RC12 PARENT RETURN 0");
        return 0;
    }
    return run_child();
}
