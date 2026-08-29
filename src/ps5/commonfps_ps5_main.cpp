/*
 * Common FPS for PS5 - RC11 snapshot-gated MDBG FPS backend probe
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

constexpr const char* kLogPath = "/data/CommonFPS_RC11_mdbg_backend.log";
constexpr int kRuntimeSeconds = 180;
constexpr int kSnapshotPeriodSeconds = 5;
constexpr int kSnapshotCount = kRuntimeSeconds / kSnapshotPeriodSeconds;
constexpr int kStableSnapshotsBeforeAttach = 2;

void log_line(const char* text) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, int pid) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "%s%d\n", prefix, pid);
    std::fclose(f);
}

bool bounded_name_equals(const std::uint8_t* record,
                         std::size_t record_size,
                         const char* wanted) {
    constexpr std::size_t kNameOffset = 447U;
    if (record_size <= kNameOffset)
        return false;

    const std::size_t wanted_len = std::strlen(wanted);
    const std::size_t available = record_size - kNameOffset;
    if (available < wanted_len + 1U)
        return false;

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
    if (sysctl(mib, 4, nullptr, &required, nullptr, 0) != 0 || required == 0)
        return out;

    const std::size_t capacity = required + 65536U;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!buffer)
        return out;

    std::size_t filled = capacity;
    if (sysctl(mib, 4, buffer, &filled, nullptr, 0) != 0 || filled > capacity) {
        std::free(buffer);
        return out;
    }

    std::uint8_t* ptr = buffer;
    std::uint8_t* end = buffer + filled;
    while (ptr < end) {
        if (static_cast<std::size_t>(end - ptr) < sizeof(int))
            break;

        int record_size_signed = 0;
        std::memcpy(&record_size_signed, ptr, sizeof(record_size_signed));
        if (record_size_signed <= 0)
            break;

        const std::size_t record_size = static_cast<std::size_t>(record_size_signed);
        if (record_size > static_cast<std::size_t>(end - ptr))
            break;

        ++out.records;
        if (record_size >= 76U) {
            int pid = -1;
            std::memcpy(&pid, ptr + 72U, sizeof(pid));
            if (out.shellui_pid < 0 && bounded_name_equals(ptr, record_size, "SceShellUI"))
                out.shellui_pid = pid;
            if (out.game_pid < 0 && bounded_name_equals(ptr, record_size, "eboot.bin"))
                out.game_pid = pid;
        }

        ptr += record_size;
    }

    out.filled = filled;
    out.rc = 0;
    std::free(buffer);
    return out;
}

void log_snapshot(int index, const SnapshotResult& s, int stable_count) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f,
                 "RC11 SNAPSHOT %d/%d rc=%d records=%zu filled=%zu shellui=%d game=%d stable=%d\n",
                 index, kSnapshotCount, s.rc, s.records, s.filled,
                 s.shellui_pid, s.game_pid, stable_count);
    std::fclose(f);
}

void log_attach_ok(int pid, std::uintptr_t counter) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "RC11 ATTACH OK pid=%d counter=0x%llx\n",
                 pid, static_cast<unsigned long long>(counter));
    std::fclose(f);
}

void log_fps(int pid, int fps) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "RC11 FPS pid=%d value=%d\n", pid, fps);
    std::fclose(f);
}

int run_child() {
    log_pid("RC11 CHILD pid=", static_cast<int>(getpid()));
    log_line("RC11 snapshot-gated v11 FPS backend using MDBG; NO renderer / NO ShellUI inject");
    log_line("RC11 one combined sysctl snapshot every 5s; one FPS read immediately after snapshot");
    log_line("RC11 new game PID must survive TWO consecutive snapshots before attach");
    log_line("RC11 NO shsrv ptrace: no PT_ATTACH / waitpid / PT_IO / PT_DETACH");
    log_line("RC11 module enumeration still uses short debugger-auth scope; sampling uses SDK v0.41 mdbg_copyout");

    common_fps::ps5::Ps5Platform platform;
    common_fps::FpsSampler sampler(platform);

    int observed_game = -2;
    int stable_count = 0;

    for (int index = 1; index <= kSnapshotCount; ++index) {
        const SnapshotResult s = take_snapshot();

        if (s.rc != 0) {
            log_snapshot(index, s, stable_count);
            log_line("RC11 SNAPSHOT FAIL; no FPS operation this cycle");
            sleep(kSnapshotPeriodSeconds);
            continue;
        }

        if (s.game_pid != observed_game) {
            observed_game = s.game_pid;
            stable_count = 1;
            sampler.reset();
            log_pid("RC11 CHANGE eboot.bin pid=", observed_game);
        } else if (stable_count < 1000) {
            ++stable_count;
        }

        log_snapshot(index, s, stable_count);

        if (observed_game <= 0) {
            sampler.reset();
            sleep(kSnapshotPeriodSeconds);
            continue;
        }

        if (stable_count < kStableSnapshotsBeforeAttach) {
            log_line("RC11 WAIT PID confirmation; no attach/sample this cycle");
            sleep(kSnapshotPeriodSeconds);
            continue;
        }

        if (!sampler.attached()) {
            log_pid("RC11 ATTACH TRY confirmed pid=", observed_game);
            if (sampler.attach(observed_game))
                log_attach_ok(observed_game, sampler.counter_address());
            else {
                log_line("RC11 ATTACH RETRY on next confirmed snapshot");
                sleep(kSnapshotPeriodSeconds);
                continue;
            }
        }

        if (sampler.attached() && sampler.pid() == observed_game) {
            const auto fps = sampler.sample();
            if (fps)
                log_fps(observed_game, *fps);
            else if (!sampler.attached())
                log_line("RC11 SAMPLE lost attachment; wait for next snapshot");
            else
                log_line("RC11 SAMPLE warmup/no-value");
        }

        sleep(kSnapshotPeriodSeconds);
    }

    sampler.reset();
    log_line("RC11 CHILD DONE clean return");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("RC11 PARENT start pid=", static_cast<int>(getpid()));

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC11 fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC11 PARENT forked child=", static_cast<int>(pid));
        log_line("RC11 PARENT RETURN 0");
        return 0;
    }

    return run_child();
}
