/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

/*
 * Call a function while the target is already ptrace-attached. The remote
 * bootstrap must execute INT3 before returning. Match the hardware-stable
 * v1.0.0 pt_call2 contract by preserving the stopped target thread's RSP and
 * RBP; the original register set is restored before this function returns.
 */
[[nodiscard]] long call_until_breakpoint_on_target_stack(
    pid_t pid,
    std::uintptr_t address,
    std::uint64_t arg0 = 0,
    std::uint64_t arg1 = 0,
    std::uint64_t arg2 = 0,
    std::uint64_t arg3 = 0,
    std::uint64_t arg4 = 0,
    std::uint64_t arg5 = 0) noexcept;

} // namespace common_fps::ps5
