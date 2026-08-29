/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace {

void stage_log(const char* text) {
    FILE* f = std::fopen("/data/CommonFPS_RC4_sysctl.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void stage_log_pid(const char* prefix, int pid) {
    FILE* f = std::fopen("/data/CommonFPS_RC4_sysctl.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s%d\n", prefix, pid);
    std::fclose(f);
}

/*
 * Read-only userland process discovery.
 *
 * This deliberately does NOT use etaHEN fps_elf/find_proc_by_name(),
 * KERNEL_ADDRESS_ALLPROC, kernel_copyout, ptrace, authid changes, or any
 * ShellUI injection. The raw offsets below are the same kinfo_proc fields
 * already used by etaHEN's own sysctl fallback: pid @ 72, tdname @ 447.
 */
int safe_find_pid(const char* wanted) {
    int mib[4] = {1, 14, 8, 0};
    std::size_t size = 0;
    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
        return -1;

    auto* buffer = static_cast<std::uint8_t*>(std::malloc(size));
    if (!buffer)
        return -1;

    std::size_t filled = size;
    if (sysctl(mib, 4, buffer, &filled, nullptr, 0) != 0) {
        std::free(buffer);
        return -1;
    }

    int found = -1;
    std::uint8_t* ptr = buffer;
    std::uint8_t* end = buffer + filled;

    while (ptr < end) {
        if (static_cast<std::size_t>(end - ptr) < sizeof(int))
            break;

        int record_size = 0;
        std::memcpy(&record_size, ptr, sizeof(record_size));
        if (record_size <= 0 ||
            static_cast<std::size_t>(record_size) > static_cast<std::size_t>(end - ptr))
            break;

        if (record_size > 448) {
            int pid = -1;
            std::memcpy(&pid, ptr + 72, sizeof(pid));
            const char* name = reinterpret_cast<const char*>(ptr + 447);
            const std::size_t available = static_cast<std::size_t>(record_size) - 447U;
            const std::size_t compare = available < 64U ? available : 64U;

            if (compare != 0 && std::strncmp(name, wanted, compare) == 0) {
                found = pid;
                break;
            }
        }

        ptr += record_size;
    }

    std::free(buffer);
    return found;
}

int run_worker() {
    stage_log("RC4 START sysctl-only; NO kernel allproc / NO ptrace / NO auth / NO inject");

    int last_shellui = -2;
    int last_game = -2;
    unsigned heartbeat = 0;

    for (;;) {
        const int shellui = safe_find_pid("SceShellUI");
        const int game = safe_find_pid("eboot.bin");

        if (shellui != last_shellui) {
            stage_log_pid("RC4 SceShellUI pid=", shellui);
            last_shellui = shellui;
        }
        if (game != last_game) {
            stage_log_pid("RC4 eboot.bin pid=", game);
            last_game = game;
        }

        if ((heartbeat++ % 10U) == 0U)
            stage_log("RC4 ALIVE");

        usleep(500000);
    }
}

} // namespace

extern "C" int main() {
    const pid_t pid = fork();
    if (pid > 0)
        return 0;
    if (pid < 0) {
        stage_log("RC4 fork FAIL; continuing in current process");
        return run_worker();
    }
    return run_worker();
}
