/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace common_fps::ps5::shellui {

/*
 * Resolve the minimal Mono/PUI runtime surface, locate the ShellUI Game
 * container scene and install one hook on PUI Application.Update.
 *
 * The hook is deliberately used only as a main-thread callback.  All FPS
 * sampling stays in the controller process and all PUI mutation stays here.
 */
bool initialize_visual_hook();

/*
 * Becomes true only after the installed Application.Update hook has actually
 * executed on a live Game RootWidget.  The controller uses this to gate the
 * normal game lifecycle.
 */
bool visual_ready();

} // namespace common_fps::ps5::shellui
