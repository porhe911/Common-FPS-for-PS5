/*
 * Common FPS v0.28b source rebuild
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Behavior reconstructed from the hardware-proven v0.28b injector and the
 * GPL ptrace helpers derived from John Tornblom's PS5 tooling.
 */

#include "pt_call_breakpoint.hpp"

#include <csignal>
#include <cstring>
#include <machine/reg.h>
#include <sys/wait.h>

extern "C" {
#include "pt.h"
}

namespace common_fps::legacy_v028b {

long call_until_breakpoint(
    pid_t pid,
    std::uintptr_t address,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5) noexcept {

    if (pid <= 0 || address == 0)
        return -1;

    reg original{};
    reg remote{};

    if (pt_getregs(pid, &original) != 0)
        return -1;

    std::memcpy(&remote, &original, sizeof(remote));
    remote.r_rip = address;
    remote.r_rdi = arg0;
    remote.r_rsi = arg1;
    remote.r_rdx = arg2;
    remote.r_rcx = arg3;
    remote.r_r8 = arg4;
    remote.r_r9 = arg5;

    if (pt_setregs(pid, &remote) != 0)
        return -1;

    // The bootstrap stager intentionally executes INT3 after pthread_create.
    // PT_CONTINUE returns immediately, so wait until the target stops again.
    if (pt_continue(pid, SIGCONT) != 0) {
        (void)pt_setregs(pid, &original);
        return -1;
    }

    if (waitpid(pid, nullptr, 0) < 0) {
        (void)pt_setregs(pid, &original);
        return -1;
    }

    reg stopped{};
    if (pt_getregs(pid, &stopped) != 0) {
        (void)pt_setregs(pid, &original);
        return -1;
    }

    if (pt_setregs(pid, &original) != 0)
        return -1;

    return static_cast<long>(stopped.r_rax);
}

} // namespace common_fps::legacy_v028b
