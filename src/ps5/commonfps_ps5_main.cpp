/*
 * Common FPS for PS5 - RC8 sparse combined-sysctl polling probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_RC8_sysctl_sparse.log";

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
                 "RC8 SNAPSHOT %d/12 rc=%d records=%zu filled=%zu shellui=%d game=%d\n",
                 index, s.rc, s.records, s.filled, s.shellui_pid, s.game_pid);
    std::fclose(f);
}

int run_child() {
    log_pid("RC8 CHILD pid=", static_cast<int>(getpid()));
    log_line("RC8 CHILD sparse polling: ONE combined process snapshot every 5s; 12 snapshots total");
    log_line("RC8 NO kernel allproc / NO ptrace / NO auth / NO inject / NO FPS");

    int last_game = -2;
    int last_shellui = -2;

    for (int i = 1; i <= 12; ++i) {
        sleep(5);
        const SnapshotResult s = take_snapshot();
        log_snapshot(i, s);

        if (s.rc == 0) {
            if (s.shellui_pid != last_shellui) {
                log_pid("RC8 CHANGE SceShellUI pid=", s.shellui_pid);
                last_shellui = s.shellui_pid;
            }
            if (s.game_pid != last_game) {
                log_pid("RC8 CHANGE eboot.bin pid=", s.game_pid);
                last_game = s.game_pid;
            }
        }
    }

    log_line("RC8 CHILD DONE clean return");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("RC8 PARENT start pid=", static_cast<int>(getpid()));

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC8 fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC8 PARENT forked child=", static_cast<int>(pid));
        log_line("RC8 PARENT RETURN 0");
        return 0;
    }

    return run_child();
}
