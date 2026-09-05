/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/wire.hpp"
#include <cstdarg>

namespace common_fps::ps5::shellui {

/* Resolve the already-loaded ShellUI Mono runtime and PUI assemblies. */
bool initialize_runtime();

/* Bind the loopback UDP receiver used by the controller. */
bool initialize_receiver();

/* Remain blocked on IPC for the complete SceShellUI process lifetime. */
[[noreturn]] void run_receiver_loop();

} // namespace common_fps::ps5::shellui
