/*
 * Common FPS for PS5 — FW 11.60 diagnostic preflight
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "common_fps/preflight_process_view.hpp"
#include "common_fps/session_log.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <sys/syscall.h>
#include <unistd.h>
extern "C" {
#include <ps5/kernel.h>
int sceKernelSetProcessName(const char* name);
}

namespace {
constexpr const char* kLog = "/data/CommonFPS_FW1160_Preflight.log";
constexpr unsigned kObserveSeconds = 120;
constexpr std::size_t kMaxProcessBytes = 4U * 1024U * 1024U;

void snapshot(unsigned tick) noexcept {
    using namespace common_fps;
    int mib[4] = {1, 14, 8, 0};
    std::size_t required = 0;
    log_durable_message(kLog, "tick=%u stage=sysctl-size-before", tick);
    errno = 0;
    const int size_rc = static_cast<int>(syscall(
        SYS___sysctl, mib, 4, nullptr, &required, nullptr, 0));
    const int size_errno = size_rc == 0 ? 0 : errno;
    log_durable_message(kLog,
        "tick=%u stage=sysctl-size-after rc=%d errno=%d bytes=%zu",
        tick, size_rc, size_errno, required);
    if (size_rc != 0 || required == 0 || required > kMaxProcessBytes)
        return;
    const std::size_t capacity = std::min(
        required + required / 4 + 4096, kMaxProcessBytes);
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!bytes) {
        log_durable_message(kLog, "tick=%u stage=allocation-failed", tick);
        return;
    }
    std::size_t count = capacity;
    log_durable_message(kLog,
        "tick=%u stage=sysctl-data-before capacity=%zu", tick, capacity);
    errno = 0;
    const int data_rc = static_cast<int>(syscall(
        SYS___sysctl, mib, 4, bytes, &count, nullptr, 0));
    const int data_errno = data_rc == 0 ? 0 : errno;
    log_durable_message(kLog,
        "tick=%u stage=sysctl-data-after rc=%d errno=%d bytes=%zu",
        tick, data_rc, data_errno, count);
    if (data_rc == 0 && count <= capacity) {
        std::size_t offset = 0;
        unsigned records = 0, malformed = 0, games = 0, shells = 0;
        while (offset < count) {
            PreflightProcessView view{};
            if (!preflight_process_view(bytes + offset, count - offset, view)) {
                ++malformed;
                break;
            }
            ++records;
            if (view.game_name_offset >= 0)
                ++games;
            if (view.shellui_name_offset >= 0)
                ++shells;
            if (view.game_name_offset >= 0 || view.shellui_name_offset >= 0 ||
                view.pid == getpid()) {
                log_durable_message(kLog,
                    "tick=%u pid=%d ppid=%d record_size=%zu name447=%.32s "
                    "eboot_name_at=%d shellui_name_at=%d",
                    tick, view.pid, view.ppid, view.size, view.legacy_name,
                    view.game_name_offset, view.shellui_name_offset);
            }
            offset += view.size;
        }
        log_durable_message(kLog,
            "tick=%u stage=parse-complete records=%u malformed=%u "
            "game_records=%u shellui_records=%u",
            tick, records, malformed, games, shells);
    }
    std::free(bytes);
}
} // namespace

extern "C" int main() {
    using namespace common_fps;
    write_session_log(kLog, "", 0, true, true);
    log_durable_message(kLog,
        "Common FPS for PS5 FW1160 Preflight v0.1\n"
        "mode=process-observation-only internal_fork=absent "
        "overlay=disabled videoout=disabled dce=disabled mdbg=disabled "
        "ptrace=disabled auth_changes=disabled log_reset=activation "
        "log_limit=65536 observe_seconds=120\n"
        "SDK payload startup remains unchanged; application loop uses sysctl only.");
    log_durable_message(kLog, "stage=identity-before pid=%d ppid=%d", getpid(), getppid());
    const int name_rc = sceKernelSetProcessName("CFPSDiag.elf");
    log_durable_message(kLog, "stage=identity-after rc=%d", name_rc);
    log_durable_message(kLog, "stage=firmware-before");
    const std::uint32_t sdk = kernel_get_fw_version();
    log_durable_message(kLog, "stage=firmware-after sdk=0x%08x", sdk);
    if ((sdk & 0xffff0000U) != 0x11600000U) {
        log_durable_message(kLog, "stage=wrong-firmware observation=skipped");
    } else {
        for (unsigned tick = 0; tick < kObserveSeconds; ++tick) {
            snapshot(tick);
            usleep(1000000);
        }
        log_durable_message(kLog, "stage=observation-complete passive-park=1");
    }
    // Keep the loader-tracked controller resident; no injected thread exists.
    for (;;)
        usleep(5000000);
}
