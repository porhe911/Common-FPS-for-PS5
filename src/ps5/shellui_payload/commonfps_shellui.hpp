/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/wire.hpp"

namespace common_fps::ps5::shellui {

bool initialize_receiver();
void shutdown_receiver();

/*
 * Call from a ShellUI-safe update/render context.
 * It applies the newest packet only when sequence changes.
 */
void apply_latest_state();

} // namespace common_fps::ps5::shellui
