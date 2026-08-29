/*
 * Common FPS for PS5 - RC7 single-sysctl-snapshot safety probe
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

constexpr const char* kLogPath = "/data/CommonFPS_RC7_sysctl_once.log";

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

/*
 * Exactly one process-list snapshot for the whole RC7 run.
 * sysctl is called once to size the buffer and once to fill it. The same
 * returned buffer is parsed for both SceShellUI and eboot.bin, unlike RC4
 * which performed independent process-list reads twice every 500 ms.
 */
void take_single_snapshot() {
    log_line("RC7 SNAPSHOT BEGIN (one process-list read only)");

    int mib[4] = {1, 14, 8, 0};
    std::size_t required = 0;
    const int size_rc = sysctl(mib, 4, nullptr, &required, nullptr, 0);
    if (size_rc != 0 || required == 0) {
        log_line("RC7 SNAPSHOT size sysctl FAIL");
        return;
    }

    // Leave headroom in case a process appears between the size and fill call.
    const std::size_t capacity = required + 65536U;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!buffer) {
        log_line("RC7 SNAPSHOT malloc FAIL");
        return;
    }

    std::size_t filled = capacity;
    const int fill_rc = sysctl(mib, 4, buffer, &filled, nullptr, 0);
    if (fill_rc != 0 || filled > capacity) {
        std::free(buffer);
        log_line("RC7 SNAPSHOT fill sysctl FAIL");
        return;
    }

    int shellui_pid = -1;
    int game_pid = -1;
    std::size_t records = 0;

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

        ++records;
        if (record_size >= 76U) {
            int pid = -1;
            std::memcpy(&pid, ptr + 72U, sizeof(pid));
            if (shellui_pid < 0 && bounded_name_equals(ptr, record_size, "SceShellUI"))
                shellui_pid = pid;
            if (game_pid < 0 && bounded_name_equals(ptr, record_size, "eboot.bin"))
                game_pid = pid;
        }

        ptr += record_size;
    }

    std::free(buffer);

    FILE* f = std::fopen(kLogPath, "a");
    if (f) {
        std::fprintf(f, "RC7 SNAPSHOT records=%zu filled=%zu\n", records, filled);
        std::fclose(f);
    }
    log_pid("RC7 SceShellUI pid=", shellui_pid);
    log_pid("RC7 eboot.bin pid=", game_pid);
    log_line("RC7 SNAPSHOT END; NO MORE sysctl calls");
}

int run_child() {
    log_pid("RC7 CHILD pid=", static_cast<int>(getpid()));
    log_line("RC7 CHILD resident; fork proven in RC6; waiting 10s before ONE sysctl snapshot");

    sleep(10);
    take_single_snapshot();

    // No more sysctl calls after the snapshot. Keep the same resident-worker
    // shape long enough to test gameplay stability, then terminate cleanly.
    for (int i = 1; i <= 10; ++i) {
        sleep(5);
        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "RC7 ALIVE %d/10 (no sysctl)\n", i);
            std::fclose(f);
        }
    }

    log_line("RC7 CHILD DONE clean return");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("RC7 PARENT start pid=", static_cast<int>(getpid()));

    const pid_t pid = fork();
    if (pid < 0) {
        log_line("RC7 fork FAIL");
        return 1;
    }

    if (pid > 0) {
        log_pid("RC7 PARENT forked child=", static_cast<int>(pid));
        log_line("RC7 PARENT RETURN 0");
        return 0;
    }

    return run_child();
}
