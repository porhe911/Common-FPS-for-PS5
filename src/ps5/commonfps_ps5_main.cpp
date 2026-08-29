/*
 * Common FPS for PS5 - RC9 sparse lifecycle + v11 FPS backend probe
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

constexpr const char* kLogPath = "/data/CommonFPS_RC9_fps_backend.log";
constexpr int kRuntimeSeconds = 120;
constexpr int kSnapshotPeriodSeconds = 5;

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
    return std::memcmp(name, wanted, wanted_len) == 0 &&
           name[wanted_len] == 0;
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

void log_snapshot(int index, const SnapshotResult& s) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f,
                 "RC9 SNAPSHOT %d rc=%d records=%zu filled=%zu shellui=%d game=%d\n",
                 index, s.rc, s.records, s.filled, s.shellui_pid, s.game_pid);
    std::fclose(f);
}

void log_attach_ok(int pid, std::uintptr_t counter) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "RC9 ATTACH OK pid=%d counter=0x%llx\n",
                 pid, static_cast<unsigned long long>(counter));
    std::fclose(f);
}

void log_fps(int pid, int fps) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f)
        return;
    std::fprintf(f, "RC9 FPS pid=%d value=%d\n", pid, fps);
    std::fclose(f);
}

int run_child() {
    log_pid("RC9 CHILD pid=", static_cast<int>(getpid()));
    log_line("RC9 sparse lifecycle + v11 FPS backend; NO renderer / NO ShellUI inject");
    log_line("RC9 process discovery = one combined sysctl snapshot every 5s");
    log_line("RC9 temporary debugger auth ONLY during sampler attach/discovery");

    common_fps::ps5::Ps5Platform platform;
    common_fps::FpsSampler sampler(platform);

    int last_game = -2;
    int current_game = -1;
    int snapshot_index = 0;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        if ((second % kSnapshotPeriodSeconds) == 0) {
            const SnapshotResult s = take_snapshot();
            log_snapshot(++snapshot_index, s);

            if (s.rc == 0) {
                current_game = s.game_pid;

                if (current_game != last_game) {
                    log_pid("RC9 CHANGE eboot.bin pid=", current_game);
                    sampler.reset();
                    last_game = current_game;
                }

                if (current_game > 0 && !sampler.attached()) {
                    log_pid("RC9 ATTACH TRY pid=", current_game);
                    if (sampler.attach(current_game))
                        log_attach_ok(current_game, sampler.counter_address());
                    else
                        log_line("RC9 ATTACH RETRY later");
                }
            }
        }

        if (sampler.attached()) {
            const int sample_pid = sampler.pid();
            const auto fps = sampler.sample();
            if (fps)
                log_fps(sample_pid, *fps);
            else if (!sampler.attached())
                log_line("RC9 SAMPLE lost attachment; waiting for next sparse snapshot");
        }

        sleep(1);
    }

    sampler.reset();
    log_line("RC9 CHILD DONE clean return");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("RC9 PARENT start pid=", static_cast<int>(getpid()));

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC9 fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC9 PARENT forked child=", static_cast<int>(pid));
        log_line("RC9 PARENT RETURN 0");
        return 0;
    }

    return run_child();
}
