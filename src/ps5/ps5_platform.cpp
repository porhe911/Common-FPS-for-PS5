/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ps5_platform.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include "proc.h"
#include "pt.h"
#include "ucred.h"

int mdbg_copyout(
    pid_t pid,
    std::uintptr_t remote,
    void* local,
    std::size_t size);

struct OrbisKernelSwVersion {
    std::uint64_t pad0;
    char version_str[0x1C];
    std::uint32_t version;
    std::uint64_t pad1;
};

int sceKernelGetProsperoSystemSwVersion(OrbisKernelSwVersion* version);
}

namespace common_fps::ps5 {

namespace {

static int firmware_short() {
    OrbisKernelSwVersion version{};
    if (sceKernelGetProsperoSystemSwVersion(&version) < 0)
        return -1;

    return static_cast<int>(version.version >> 16);
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

std::optional<ProcessId> find_pid_sysctl(const char* wanted) {
    int mib[4] = {1, 14, 8, 0};
    std::size_t required = 0;
    if (sysctl(mib, 4, nullptr, &required, nullptr, 0) != 0 || required == 0)
        return std::nullopt;

    const std::size_t capacity = required + 65536U;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!buffer)
        return std::nullopt;

    std::size_t filled = capacity;
    if (sysctl(mib, 4, buffer, &filled, nullptr, 0) != 0 || filled > capacity) {
        std::free(buffer);
        return std::nullopt;
    }

    std::optional<ProcessId> result;
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

        if (record_size >= 76U && bounded_name_equals(ptr, record_size, wanted)) {
            int pid = -1;
            std::memcpy(&pid, ptr + 72U, sizeof(pid));
            if (pid > 0)
                result = pid;
            break;
        }

        ptr += record_size;
    }

    std::free(buffer);
    return result;
}

} // namespace

std::optional<ProcessId> Ps5Platform::find_game_process() {
    // RC8-proven userland discovery; never walk KERNEL_ADDRESS_ALLPROC here.
    return find_pid_sysctl("eboot.bin");
}

bool Ps5Platform::process_alive(ProcessId pid) {
    /*
     * RC9 deliberately avoids a second process-list scan for every FPS sample.
     * The controller owns game lifecycle through sparse sysctl snapshots. If a
     * process exits between snapshots, read_memory() fails and FpsSampler resets.
     */
    return pid > 0;
}

bool Ps5Platform::begin_process_inspection(ProcessId) {
    if (inspection_active_)
        return true;

    /*
     * FW 9.60 hardware parity v5-v11:
     * without debugger auth SYS_dl_get_list returns zero modules for the game;
     * with DEBUG_AUTHID the native VideoOut module/DMAP is visible.
     * Keep this privilege only for the short attach/discovery window.
     */
    saved_authid_ = set_ucred_to_debugger();
    if (saved_authid_ == 0)
        return false;

    inspection_active_ = true;
    return true;
}

void Ps5Platform::end_process_inspection(ProcessId) {
    if (!inspection_active_)
        return;

    set_proc_authid(getpid(), saved_authid_);
    saved_authid_ = 0;
    inspection_active_ = false;
}

std::optional<ModuleInfo>
Ps5Platform::find_module(ProcessId pid, const char* module_name) {
    module_info_t* module = get_module_info(pid, module_name);
    if (!module)
        return std::nullopt;

    ModuleInfo result;
    result.base = static_cast<std::uintptr_t>(module->sections[0].vaddr);
    result.name = module->filename;

    std::free(module);
    return result;
}

bool Ps5Platform::read_memory(
    ProcessId pid,
    std::uintptr_t address,
    void* out,
    std::size_t size) {

    const int fw = firmware_short();
    if (fw < 0)
        return false;

    if (fw >= 0x840) {
        if (pt_attach(pid) < 0)
            return false;

        const int rc = pt_copyout(pid, address, out, size);
        pt_detach(pid, 0);
        return rc >= 0;
    }

    return mdbg_copyout(pid, address, out, size) >= 0;
}

std::uint64_t Ps5Platform::monotonic_us() {
    timeval tv{};
    gettimeofday(&tv, nullptr);

    return
        static_cast<std::uint64_t>(tv.tv_sec) * 1'000'000ULL +
        static_cast<std::uint64_t>(tv.tv_usec);
}

void Ps5Platform::sleep_ms(unsigned milliseconds) {
    usleep(static_cast<useconds_t>(milliseconds) * 1000U);
}

} // namespace common_fps::ps5
