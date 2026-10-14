/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

// Reuse this state for a rollback at the SAME stopped PID/address. It retains
// the initial protection even if the first restore fails; never snapshot RWX
// as the original protection on the rollback attempt.
struct ShellUiGuardWriteState {
    pid_t pid = -1;
    std::uintptr_t address = 0;
    int original_protection = -1;
    int protect_rc = -1;
    int mdbg_write_rc = -1;
    int restore_protection_rc = -1;
    bool protection_restored = false;
};

// Caller must stop ShellUI and verify all expected code bytes first. The SDK
// changes the containing VM entry, not a hardware page. Only ONE code byte is
// written; the exact original VM protection is restored and checked before
// returning to the caller's code verification/detach.
int shellui_write_guard_byte(pid_t pid, std::uintptr_t address,
                            std::uint8_t byte,
                            ShellUiGuardWriteState& state) noexcept;
} // namespace common_fps::ps5
