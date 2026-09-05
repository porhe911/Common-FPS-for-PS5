/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdarg>
#include <sys/types.h>

namespace common_fps::ps5 {

/* Ensure the source-built renderer is resident in the current SceShellUI. */
bool ensure_shellui_renderer();

/* Discover ShellUI without attaching, injecting or changing the process. */
pid_t observe_shellui_pid() noexcept;

/* Last ShellUI PID observed by the read-only discovery path. */
pid_t shellui_renderer_pid() noexcept;

} // namespace common_fps::ps5
