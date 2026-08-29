/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ps5_platform.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include "proc.h"
#include "ucred.h"

int mdbg_copyout(int pid, unsigned long addr, void* buf, unsigned long len);
}

namespace common_fps::ps5 {

namespace {

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
    return find_pid_sysctl("eboot.bin");
}

bool Ps5Platform::process_alive(ProcessId pid) {
    return pid > 0;
}

bool Ps5Platform::begin_process_inspection(ProcessId) {
    if (inspection_active_)
        return true;

    /* Module enumeration needs temporary debugger auth on FW 9.60. */
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

    /*
     * RC11: no shsrv ptrace at all. mdbg_copyout() is provided by the pinned
     * PS5 Payload SDK v0.41 CRT and reads process memory through SYS_mdbg_call.
     * This avoids PT_ATTACH/waitpid/PT_IO/PT_DETACH on every FPS sample.
     */
    return mdbg_copyout(
               static_cast<int>(pid),
               static_cast<unsigned long>(address),
               out,
               static_cast<unsigned long>(size)) == 0;
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
