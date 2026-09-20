/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/shellui_hook_protocol.hpp"

#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

enum class ShellUiHookPollResult {
    NoRequest,
    Applied,
    Failed,
};

struct ShellUiHookPatchReport {
    ShellUiHookStatus status = ShellUiHookStatus::InvalidRequest;
    std::uint32_t sdk_version = 0;
    std::uint64_t method_address = 0;
    std::uint32_t displaced_size = 0;
    int read_rc = -1;
    int write_rc = -1;
    bool expected_matched = false;
    bool verified = false;
    bool restored = false;
    bool detached = false;
    bool auth_restored = false;
};

/* Remove stale Stage 8.2 request/ack files before a new renderer starts. */
void clear_shellui_hook_protocol_files() noexcept;

/*
 * Apply one native Application.Update patch while SceShellUI is stopped.
 * No game process is written and unsupported firmware fails closed.
 */
[[nodiscard]] ShellUiHookPollResult poll_and_apply_shellui_hook(
    pid_t shellui_pid,
    ShellUiHookPatchReport& report) noexcept;

} // namespace common_fps::ps5
