/*
 * Common FPS v0.28b source rebuild
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The control flow is independently reconstructed from the exact stable
 * v0.28b ELF. ELF mapping/payload-args and ptrace primitives come from the
 * pinned GPL ps5-payload-dev/shsrv v0.20 dependency.
 */

#include "stable_injector.hpp"
#include "pt_call_breakpoint.hpp"

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
#include <ps5/nid.h>
#include "pt.h"

intptr_t commonfps_v028b_elfldr_load(pid_t pid, std::uint8_t* elf);
intptr_t commonfps_v028b_elfldr_payload_args(pid_t pid);
}

namespace common_fps::legacy_v028b {
namespace {

constexpr std::uint64_t kPtraceAuthId = 0x4800000000010003ULL;

using RemotePthreadCreate = int (*)(
    std::uintptr_t*,
    const void*,
    void* (*)(void*),
    void*);

struct RemoteBootstrapFunctions {
    void* debug_out = nullptr;             // +0x00 (reference parity)
    void* elf_main = nullptr;              // +0x08
    void* payload_args = nullptr;          // +0x10
    RemotePthreadCreate pthread_create_fn = nullptr; // +0x18
    int pthread_create_rc = -1;            // +0x20
    std::uint32_t padding = 0;              // +0x24
};

static_assert(offsetof(RemoteBootstrapFunctions, elf_main) == 0x08);
static_assert(offsetof(RemoteBootstrapFunctions, payload_args) == 0x10);
static_assert(offsetof(RemoteBootstrapFunctions, pthread_create_fn) == 0x18);
static_assert(offsetof(RemoteBootstrapFunctions, pthread_create_rc) == 0x20);
static_assert(sizeof(RemoteBootstrapFunctions) == 0x28);

extern "C" __attribute__((used, noinline, section(".commonfps_stager.1")))
int commonfps_v028b_stager(RemoteBootstrapFunctions* functions) {
    std::uintptr_t thread = 0;
    functions->pthread_create_rc = functions->pthread_create_fn(
        &thread,
        nullptr,
        reinterpret_cast<void* (*)(void*)>(functions->elf_main),
        functions->payload_args);

    asm volatile("int3");
    return 0;
}

extern "C" __attribute__((used, noinline, section(".commonfps_stager.2")))
int commonfps_v028b_stager_end() {
    return 0;
}

std::size_t stager_size() noexcept {
    const auto begin = reinterpret_cast<std::uintptr_t>(&commonfps_v028b_stager);
    const auto end = reinterpret_cast<std::uintptr_t>(&commonfps_v028b_stager_end);
    return end > begin ? static_cast<std::size_t>(end - begin) : 0;
}

std::uintptr_t resolve_remote(pid_t pid, const char* symbol) noexcept {
    char nid[12]{};
    if (!nid_encode(symbol, nid))
        return 0;
    return static_cast<std::uintptr_t>(pt_resolve(pid, nid));
}

} // namespace

InjectionResult inject_renderer_once(
    pid_t shellui_pid,
    const std::uint8_t* renderer_elf,
    std::size_t renderer_size) noexcept {

    InjectionResult result{};
    if (shellui_pid <= 0 || !renderer_elf || renderer_size < 64)
        return result;

    // v0.28b leaves the producer child privileged after startup. This is a
    // one-time transition, not a per-sample credential toggle.
    const pid_t self = getpid();
    (void)kernel_set_ucred_authid(self, kPtraceAuthId);

    if (pt_attach(shellui_pid) < 0)
        return result;
    result.attached = true;

    bool should_resume = true;

    const std::uintptr_t pthread_create =
        resolve_remote(shellui_pid, "pthread_create");
    if (pthread_create == 0)
        goto detach;

    const std::uintptr_t debug_out =
        resolve_remote(shellui_pid, "sceKernelDebugOutText");

    const intptr_t entry = commonfps_v028b_elfldr_load(
        shellui_pid,
        const_cast<std::uint8_t*>(renderer_elf));
    if (entry <= 0)
        goto detach;
    result.elf_loaded = true;

    const intptr_t payload_args =
        commonfps_v028b_elfldr_payload_args(shellui_pid);
    if (payload_args <= 0)
        goto detach;
    result.payload_args_ready = true;

    const std::size_t code_size = stager_size();
    if (code_size == 0)
        goto detach;

    const intptr_t remote_stager = pt_mmap(
        shellui_pid,
        0,
        code_size,
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0);
    if (remote_stager <= 0)
        goto detach;

    if (kernel_mprotect(
            shellui_pid,
            remote_stager,
            code_size,
            PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        goto detach;
    }

    if (pt_copyin(
            shellui_pid,
            reinterpret_cast<const void*>(&commonfps_v028b_stager),
            remote_stager,
            code_size) != 0) {
        goto detach;
    }

    RemoteBootstrapFunctions functions{};
    functions.debug_out = reinterpret_cast<void*>(debug_out);
    functions.elf_main = reinterpret_cast<void*>(entry);
    functions.payload_args = reinterpret_cast<void*>(payload_args);
    functions.pthread_create_fn =
        reinterpret_cast<RemotePthreadCreate>(pthread_create);
    functions.pthread_create_rc = -1;

    const intptr_t remote_functions = pt_mmap(
        shellui_pid,
        0,
        sizeof(functions),
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0);
    if (remote_functions <= 0)
        goto detach;

    if (pt_copyin(
            shellui_pid,
            &functions,
            remote_functions,
            sizeof(functions)) != 0) {
        goto detach;
    }

    (void)call_until_breakpoint(
        shellui_pid,
        static_cast<std::uintptr_t>(remote_stager),
        static_cast<std::uint64_t>(remote_functions));
    result.bootstrap_started = true;

    // Exact stable payload includes this readback guard: the remote stager
    // stores pthread_create's return code at +0x20 before INT3.
    RemoteBootstrapFunctions readback{};
    if (pt_copyout(
            shellui_pid,
            remote_functions,
            &readback,
            sizeof(readback)) == 0) {
        result.pthread_create_rc = readback.pthread_create_rc;
        result.pthread_create_ok = (readback.pthread_create_rc == 0);
    }

detach:
    (void)pt_detach(shellui_pid, 0);
    if (should_resume)
        (void)kill(shellui_pid, SIGCONT);
    return result;
}

} // namespace common_fps::legacy_v028b
