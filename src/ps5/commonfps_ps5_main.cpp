/*
 * Common FPS for PS5 - RC13 fresh-child discovery isolation probe
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
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_RC13_fresh_child_discovery.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kSnapshotPeriodSeconds = 5;
constexpr int kSnapshotCount = kRuntimeSeconds / kSnapshotPeriodSeconds;
constexpr int kStableSnapshotsBeforeDiscovery = 2;

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

bool bounded_name_equals(const std::uint8_t* record,
                         std::size_t record_size,
                         const char* wanted) {
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
    if (sysctl(mib, 4, nullptr, &required, nullptr, 0) != 0 || required == 0)
        return out;

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
            if (out.shellui_pid < 0 &&
                bounded_name_equals(ptr, record_size, "SceShellUI")) {
                out.shellui_pid = pid;
            }
            if (out.game_pid < 0 &&
                bounded_name_equals(ptr, record_size, "eboot.bin")) {
                out.game_pid = pid;
            }
        }
        ptr += record_size;
    }

    out.filled = filled;
    out.rc = 0;
    std::free(buffer);
    return out;
}

void log_snapshot(int index,
                  const SnapshotResult& s,
                  int stable_count,
                  int serviced_pid) {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(
        f,
        "RC13 SNAPSHOT %d/%d rc=%d records=%zu filled=%zu shellui=%d game=%d stable=%d serviced=%d\n",
        index,
        kSnapshotCount,
        s.rc,
        s.records,
        s.filled,
        s.shellui_pid,
        s.game_pid,
        stable_count,
        serviced_pid);
    std::fclose(f);
}

[[noreturn]] void run_discovery_child(int target_pid) {
    const int self_pid = static_cast<int>(getpid());
    FILE* f = std::fopen(kLogPath, "a");
    if (f) {
        std::fprintf(f,
                     "RC13 DISCOVERY CHILD start self=%d target=%d; fresh process credentials\n",
                     self_pid,
                     target_pid);
        std::fclose(f);
    }

    common_fps::ps5::Ps5Platform platform;
    common_fps::FpsSampler sampler(platform);

    const bool ok = sampler.attach(target_pid);
    f = std::fopen(kLogPath, "a");
    if (f) {
        if (ok) {
            std::fprintf(f,
                         "RC13 DISCOVERY CHILD OK self=%d target=%d counter=0x%llx\n",
                         self_pid,
                         target_pid,
                         static_cast<unsigned long long>(sampler.counter_address()));
        } else {
            std::fprintf(f,
                         "RC13 DISCOVERY CHILD FAIL self=%d target=%d\n",
                         self_pid,
                         target_pid);
        }
        std::fprintf(f,
                     "RC13 DISCOVERY CHILD EXIT self=%d target=%d; no sampling, no reuse\n",
                     self_pid,
                     target_pid);
        std::fclose(f);
    }

    sampler.reset();
    _exit(ok ? 0 : 2);
}

int run_supervisor() {
    log_pid("RC13 SUPERVISOR pid=", static_cast<int>(getpid()));
    log_line("RC13 supervisor uses sysctl only; it never creates Ps5Platform/FpsSampler");
    log_line("RC13 each confirmed game gets one FRESH short-lived discovery child");
    log_line("RC13 discovery child exits immediately after counter discovery; ZERO FPS sampling");
    log_line("RC13 no discovery retry for the same PID, even on failure");

    int observed_game = -2;
    int stable_count = 0;
    int serviced_pid = -1;

    for (int index = 1; index <= kSnapshotCount; ++index) {
        const SnapshotResult s = take_snapshot();
        if (s.rc != 0) {
            log_snapshot(index, s, stable_count, serviced_pid);
            sleep(kSnapshotPeriodSeconds);
            continue;
        }

        if (s.game_pid != observed_game) {
            observed_game = s.game_pid;
            stable_count = 1;
            log_pid("RC13 CHANGE eboot.bin pid=", observed_game);
        } else if (stable_count < 1000) {
            ++stable_count;
        }

        log_snapshot(index, s, stable_count, serviced_pid);

        if (observed_game > 0 &&
            stable_count >= kStableSnapshotsBeforeDiscovery &&
            serviced_pid != observed_game) {

            serviced_pid = observed_game; // Never retry privileged discovery for this PID.
            log_pid("RC13 FORK fresh discovery child for target pid=", observed_game);

            const pid_t child = fork();
            if (child < 0) {
                log_line("RC13 discovery fork FAIL; marked serviced to avoid retry storm");
            } else if (child == 0) {
                run_discovery_child(observed_game);
            } else {
                int status = 0;
                const pid_t waited = waitpid(child, &status, 0);
                FILE* f = std::fopen(kLogPath, "a");
                if (f) {
                    std::fprintf(f,
                                 "RC13 SUPERVISOR reaped child=%d waitpid=%d status=0x%x target=%d\n",
                                 static_cast<int>(child),
                                 static_cast<int>(waited),
                                 status,
                                 observed_game);
                    std::fclose(f);
                }
            }
        }

        sleep(kSnapshotPeriodSeconds);
    }

    log_line("RC13 SUPERVISOR DONE clean return");
    return 0;
}
} // namespace

extern "C" int main() {
    log_pid("RC13 PARENT start pid=", static_cast<int>(getpid()));

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC13 supervisor fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC13 PARENT forked supervisor=", static_cast<int>(pid));
        log_line("RC13 PARENT RETURN 0");
        return 0;
    }

    return run_supervisor();
}
