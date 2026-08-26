/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * PS5 adapter for Common FPS.
 *
 * Upstream basis:
 *   etaHEN/Source Code/fps_elf/src/proc.cpp
 *   etaHEN util/libhijacker process-read helpers
 *
 * This file is GPL-3.0-or-later and is intended to be built against
 * the etaHEN Plugin SDK / PS5 Payload SDK.
 */

#include "ps5_platform.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

/*
 * The clean build supplies etaHEN's public proc.h in the include path.
 * It defines struct proc and module_info_t, plus these helper functions.
 */
extern "C" {
#include "proc.h"
#include "pt.h"

/*
 * mdbg_copyout is supplied by the PS5 Payload SDK runtime.
 * shsrv/pt.h supplies pt_attach(), pt_detach() and pt_copyout().
 */
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

static int firmware_short() {
    OrbisKernelSwVersion version{};
    if (sceKernelGetProsperoSystemSwVersion(&version) < 0)
        return -1;

    return static_cast<int>(version.version >> 16);
}

std::optional<ProcessId> Ps5Platform::find_game_process() {
    /*
     * Stable Common FPS v1.0.0 searched for "eboot.bin".
     * etaHEN publishes find_proc_by_name() under GPL.
     */
    struct proc* p = find_proc_by_name("eboot.bin");
    if (!p)
        return std::nullopt;

    const ProcessId pid = p->pid;
    std::free(p);
    return pid;
}

bool Ps5Platform::process_alive(ProcessId pid) {
    struct proc* p = get_proc_by_pid(pid);
    if (!p)
        return false;

    std::free(p);
    return true;
}

std::optional<ModuleInfo>
Ps5Platform::find_module(ProcessId pid, const char* module_name) {
    module_info_t* module = get_module_info(pid, module_name);
    if (!module)
        return std::nullopt;

    ModuleInfo result;
    result.base = static_cast<std::uintptr_t>(
        module->sections[0].vaddr);
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
     * etaHEN's current GPL source uses the ptrace copyout path on
     * firmware >= 0x840 and mdbg_copyout below that threshold.
     *
     * FW 9.60 therefore follows the pt_* path.
     *
     * This conservative alpha implementation uses a short
     * attach -> read -> detach window. It never writes to game memory.
     * Hardware testing is required before calling this adapter stable.
     */
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
