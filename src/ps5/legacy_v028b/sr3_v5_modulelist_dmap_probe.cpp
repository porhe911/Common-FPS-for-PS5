/*
 * Common FPS v0.28b stable-source rebuild - SR3 v5 module-list + DMAP probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SR3 keeps the hardware-stable SR2 lifecycle/auth window but replaces
 * kernel_dynlib_handle() with the exact module-list syscalls proven by the
 * FW 9.60 v5 debugger-auth probe: SYS_dl_get_list + SYS_dl_get_info_2.
 * Auth is restored before every DMAP table/root/counter read.
 */

#include "process_sysctl.hpp"
#include "proc_rw_v960.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_SR3_v5_modulelist_dmap.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint32_t kMaxTenthsFps = 3000U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;

constexpr std::uint64_t kDebuggerAuthId = 0x4800000000000006ULL;
constexpr std::uintptr_t kAuthIdOffset = 0x58ULL;

constexpr long kSysDlGetList = 0x217;
constexpr long kSysDlGetInfo2 = 0x2cd;
constexpr char kVideoOutModule[] = "libSceVideoOut.sprx";
constexpr std::uintptr_t kProbeTableOffset = 0x34980ULL;
constexpr std::size_t kProbeTableSize = 0xA8ULL;
constexpr std::size_t kProbeEntrySize = 0x18ULL;
constexpr std::size_t kProbeEntryCount = kProbeTableSize / kProbeEntrySize;
constexpr std::uintptr_t kCounterOffset = 0x768ULL;

constexpr std::size_t kModuleNameLength = 128;
constexpr std::size_t kSandboxPathLength = 1024;
constexpr std::size_t kMaxSections = 4;
constexpr std::size_t kFingerprintLength = 20;

struct ModuleSectionCompat {
    std::uint64_t vaddr;
    std::uint32_t size;
    std::uint32_t prot;
};

struct ModuleInfoCompat {
    char filename[kModuleNameLength];
    std::uint64_t handle;
    std::uint8_t unknown0[32];
    std::uint64_t init;
    std::uint64_t fini;
    std::uint64_t eh_frame_hdr;
    std::uint64_t eh_frame_hdr_sz;
    std::uint64_t eh_frame;
    std::uint64_t eh_frame_sz;
    ModuleSectionCompat sections[kMaxSections];
    std::uint8_t unknown7[1176];
    std::uint8_t fingerprint[kFingerprintLength];
    std::uint32_t unknown8;
    char libname[kModuleNameLength];
    std::uint32_t unknown9;
    char sandboxed_path[kSandboxPathLength];
    std::uint64_t sdk_version;
};

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
    std::fprintf(f, "SR3 %s auth=0x%016llx\n", tag,
                 static_cast<unsigned long long>(value));
    std::fclose(f);
}

void log_list(const char* stage, long rc, std::size_t count) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR3 MODULE %s rc=%ld count=%zu\n", stage, rc, count);
    std::fclose(f);
}

void log_module(std::size_t index, const ModuleInfoCompat& info) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f,
                 "SR3 MODULE FOUND index=%zu name=%s handle=0x%llx base=0x%llx sdk=0x%llx\n",
                 index,
                 info.filename,
                 static_cast<unsigned long long>(info.handle),
                 static_cast<unsigned long long>(info.sections[0].vaddr),
                 static_cast<unsigned long long>(info.sdk_version));
    std::fclose(f);
}

void log_counter(pid_t pid, std::uintptr_t address) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR3 COUNTER READY pid=%d address=0x%llx\n",
                 static_cast<int>(pid),
                 static_cast<unsigned long long>(address));
    std::fclose(f);
}

void log_table_entry(std::size_t index, std::uint32_t enabled, std::uint64_t pointer) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR3 TABLE active index=%zu enabled=%u pointer=0x%llx\n",
                 index, enabled, static_cast<unsigned long long>(pointer));
    std::fclose(f);
}

void log_fps(pid_t pid, std::uint32_t tenths) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR3 FPS pid=%d value=%u.%u\n",
                 static_cast<int>(pid), tenths / 10U, tenths % 10U);
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

std::optional<std::uintptr_t> find_videoout_base_v5(pid_t game_pid) noexcept {
    const std::uintptr_t self_ucred = kernel_get_proc_ucred(getpid());
    if (self_ucred == 0) {
        log_line("SR3 AUTH failed: kernel_get_proc_ucred(self)=0");
        return std::nullopt;
    }

    std::uint64_t saved_auth = 0;
    if (kernel_copyout(self_ucred + kAuthIdOffset, &saved_auth, sizeof(saved_auth)) != 0) {
        log_line("SR3 AUTH failed: read current Auth ID");
        return std::nullopt;
    }
    log_auth("AUTH saved", saved_auth);

    const std::uint64_t debug_auth = kDebuggerAuthId;
    if (kernel_copyin(&debug_auth, self_ucred + kAuthIdOffset, sizeof(debug_auth)) != 0) {
        log_line("SR3 AUTH failed: set debugger Auth ID");
        return std::nullopt;
    }
    log_line("SR3 AUTH debugger active for v5 module-list discovery only");

    std::optional<std::uintptr_t> result;
    std::size_t count = 0;
    const long rc_size = syscall(kSysDlGetList, game_pid, nullptr, 0, &count);
    log_list("list-size", rc_size, count);

    if (rc_size == 0 && count > 0 && count < 1024) {
        auto* handles = static_cast<std::uintptr_t*>(
            std::calloc(count, sizeof(std::uintptr_t)));
        if (!handles) {
            log_line("SR3 MODULE calloc handles failed");
        } else {
            std::size_t returned = count;
            const long rc_fill = syscall(kSysDlGetList, game_pid, handles, count, &returned);
            log_list("list-fill", rc_fill, returned);

            if (rc_fill == 0 && returned <= count) {
                for (std::size_t i = 0; i < returned; ++i) {
                    ModuleInfoCompat info{};
                    const long rc_info = syscall(
                        kSysDlGetInfo2, game_pid, 1, handles[i], &info);
                    if (rc_info != 0)
                        continue;

                    info.filename[kModuleNameLength - 1] = '\0';
                    if (std::strcmp(info.filename, kVideoOutModule) == 0) {
                        log_module(i, info);
                        if (info.sections[0].vaddr != 0)
                            result = static_cast<std::uintptr_t>(info.sections[0].vaddr);
                        break;
                    }
                }
            }
            std::free(handles);
        }
    }

    if (kernel_copyin(&saved_auth, self_ucred + kAuthIdOffset, sizeof(saved_auth)) != 0) {
        log_line("SR3 AUTH RESTORE FAILED");
        return std::nullopt;
    }
    log_line("SR3 AUTH restored before DMAP reads");

    if (!result)
        log_line("SR3 MODULE libSceVideoOut.sprx not found");
    return result;
}

std::optional<std::uintptr_t> resolve_counter_from_base(
    pid_t game_pid,
    std::uintptr_t module_base) noexcept {
    using common_fps::legacy_v028b::proc_read;

    std::array<std::uint8_t, kProbeTableSize> table{};
    const std::uintptr_t table_address = module_base + kProbeTableOffset;
    if (!proc_read(game_pid, table_address, table.data(), table.size())) {
        log_line("SR3 DMAP table read FAILED");
        return std::nullopt;
    }
    log_line("SR3 DMAP table read OK");

    for (std::size_t i = 0; i < kProbeEntryCount; ++i) {
        const auto* entry = table.data() + i * kProbeEntrySize;
        std::uint32_t enabled = 0;
        std::uint64_t pointer = 0;
        std::memcpy(&enabled, entry + 0x00, sizeof(enabled));
        std::memcpy(&pointer, entry + 0x08, sizeof(pointer));
        if (enabled == 0 || pointer == 0)
            continue;

        log_table_entry(i, enabled, pointer);

        std::uint64_t root = 0;
        if (!proc_read(game_pid, static_cast<std::uintptr_t>(pointer), &root, sizeof(root))) {
            log_line("SR3 DMAP root read FAILED");
            continue;
        }
        if (root == 0) {
            log_line("SR3 DMAP root is zero");
            continue;
        }

        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "SR3 DMAP root=0x%llx\n",
                         static_cast<unsigned long long>(root));
            std::fclose(f);
        }
        return static_cast<std::uintptr_t>(root) + kCounterOffset;
    }

    log_line("SR3 TABLE no usable active entry");
    return std::nullopt;
}

int run_probe() noexcept {
    using namespace common_fps::legacy_v028b;

    log_pid("SR3 CHILD pid=", getpid());
    log_line("SR3 START v5 module-list + recovered DMAP FPS probe");
    log_line("SR3 NO ptrace / NO MDBG / NO renderer / NO ShellUI inject");
    log_line("SR3 short debugger Auth only for SYS_dl_get_list/SYS_dl_get_info_2");

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
            log_pid("SR3 CHANGE eboot.bin pid=", active_pid);
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

            log_line("SR3 DISCOVERY TRY exact v5 module-list path");
            const auto module_base = find_videoout_base_v5(active_pid);
            if (!module_base) {
                log_line("SR3 DISCOVERY module stage failed; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }

            const auto resolved = resolve_counter_from_base(active_pid, *module_base);
            if (!resolved) {
                log_line("SR3 DISCOVERY DMAP counter stage failed; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }
            counter_address = *resolved;
            log_counter(active_pid, counter_address);
        }

        std::uint32_t current_counter = 0;
        if (!proc_read(active_pid, counter_address, &current_counter, sizeof(current_counter))) {
            log_line("SR3 COUNTER DMAP read failed; drop counter and rediscover later");
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
            log_line("SR3 BASELINE captured");
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
                log_line("SR3 FPS sanity reject >300.0");
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    log_line("SR3 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR3 PARENT start pid=", getpid());

    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR3 PARENT forked child=", child);
        log_line("SR3 PARENT RETURN 0");
        return 0;
    }
    if (child < 0)
        log_line("SR3 fork failed; run in current process");

    return run_probe();
}
