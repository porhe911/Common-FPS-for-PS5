/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "common_fps/platform.hpp"
#include "common_fps/renderer.hpp"
#include "common_fps/types.hpp"

namespace common_fps {

int run(Platform& platform, Renderer& renderer, const OverlayConfig& cfg);

} // namespace common_fps
