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
 * Draw the stable default "FPS: loading" pair immediately after the live
 * ShellUI Game.RootWidget becomes usable.  This is both the normal startup
 * state and a hardware diagnostic boundary: VisualReady is not emitted until
 * these Common FPS widgets were actually created.
 */
bool show_loading_state();

/*
 * Call from a ShellUI-safe update/render context.
 * It applies the newest packet only when sequence changes.
 */
void apply_latest_state();

} // namespace common_fps::ps5::shellui
