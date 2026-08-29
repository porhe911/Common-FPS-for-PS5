/*
 * Common FPS for PS5
 * Stable v1.0.0 PS5 platform adapter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "v1_stable_ps5_platform.hpp"

#include "common_fps/v1_stable_memory.hpp"

#include <cstdint>
#include <cstring>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <vector>

namespace common_fps::ps5 {
namespace {

/*
 * Hardware-parity process discovery.
 *
 * The stable Common FPS line (v0.7 -> v0.17 -> v0.22c -> v1.0.0) found the
 * foreground game through the ordinary PS5/FreeBSD KERN_PROC sysctl snapshot.
 * Do not replace this with etaHEN's allproc find_proc_by_name(): the latter was
 * introduced only by the later clean rewrite and failed the FW 9.60 parity
 * probe even while eboot.bin was already running.
 */
std::optional<ProcessId> find_process_sysctl(const char* process_name) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};

    std::size_t buffer_size = 0;
    if (sysctl(mib, 4, nullptr, &buffer_size, nullptr, 0) != 0 ||
        buffer_size == 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> buffer(buffer_size);
    if (sysctl(
            mib,
            4,
            buffer.data(),
            &buffer_size,
            nullptr,
            0) != 0) {
        return std::nullopt;
    }

    auto* cursor = buffer.data();
    auto* end = buffer.data() + buffer_size;

    while (cursor + sizeof(int) <= end) {
        const auto* info = reinterpret_cast<const struct kinfo_proc*>(cursor);
        const auto record_size = static_cast<std::size_t>(info->ki_structsize);

        if (record_size < sizeof(struct kinfo_proc) ||
            record_size > static_cast<std::size_t>(end - cursor)) {
            break;
        }

        if (info->ki_pid > 0 &&
            std::strncmp(
                info->ki_comm,
                process_name,
                sizeof(info->ki_comm)) == 0) {
            return static_cast<ProcessId>(info->ki_pid);
        }

        cursor += record_size;
    }

    return std::nullopt;
}

bool process_alive_sysctl(ProcessId pid) {
    if (pid <= 0)
        return false;

    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};

    std::size_t buffer_size = 0;
    if (sysctl(mib, 4, nullptr, &buffer_size, nullptr, 0) != 0 ||
        buffer_size == 0) {
        return false;
    }

    std::vector<std::uint8_t> buffer(buffer_size);
    if (sysctl(
            mib,
            4,
            buffer.data(),
            &buffer_size,
            nullptr,
            0) != 0) {
        return false;
    }

    auto* cursor = buffer.data();
    auto* end = buffer.data() + buffer_size;

    while (cursor + sizeof(int) <= end) {
        const auto* info = reinterpret_cast<const struct kinfo_proc*>(cursor);
        const auto record_size = static_cast<std::size_t>(info->ki_structsize);

        if (record_size < sizeof(struct kinfo_proc) ||
            record_size > static_cast<std::size_t>(end - cursor)) {
            break;
        }

        if (info->ki_pid == pid)
            return true;

        cursor += record_size;
    }

    return false;
}

} // namespace

std::optional<ProcessId> V1StablePs5Platform::find_game_process() {
    return find_process_sysctl("eboot.bin");
}

bool V1StablePs5Platform::process_alive(ProcessId pid) {
    return process_alive_sysctl(pid);
}

std::optional<ModuleInfo>
V1StablePs5Platform::find_module(ProcessId pid, const char* module_name) {
    return base_.find_module(pid, module_name);
}

bool V1StablePs5Platform::read_memory(
    ProcessId pid,
    std::uintptr_t address,
    void* out,
    std::size_t size) {

    return common_fps::v1_stable::proc_read_dmap(
        dmap_,
        pid,
        address,
        out,
        size);
}

std::uint64_t V1StablePs5Platform::monotonic_us() {
    return base_.monotonic_us();
}

void V1StablePs5Platform::sleep_ms(unsigned milliseconds) {
    base_.sleep_ms(milliseconds);
}

} // namespace common_fps::ps5
