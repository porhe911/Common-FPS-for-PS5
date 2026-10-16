/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "common_fps/types.hpp"

namespace common_fps {

Vec2 compute_anchor(const OverlayConfig& cfg);

struct OverlayLayout {
    Vec2 anchor{};
    int font_size = kDefaultFontSize;
    float label_advance = 0;
    float width = 0;
    float height = 0;
};

// Scene dimensions are PUI coordinates, never HDMI pixels applied twice.
OverlayLayout compute_layout(const OverlayConfig& cfg, float scene_width,
                             float scene_height, bool loading = false);

} // namespace common_fps
