/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdarg>

namespace common_fps::ps5 {

/* Ensure the source-built renderer is resident in the current SceShellUI. */
bool ensure_shellui_renderer();

} // namespace common_fps::ps5
