/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/wire.hpp"

namespace common_fps::ps5::shellui {

/* Resolve the already-loaded ShellUI Mono runtime and PUI assemblies. */
bool initialize_runtime();

/* Start the loopback UDP receiver used by the controller. */
bool initialize_receiver();
void shutdown_receiver();

/*
 * Queue the newest state onto ShellUI's native UI event queue.
 * All PUI mutation itself happens from the queued callback.
 */
void apply_latest_state();

} // namespace common_fps::ps5::shellui
