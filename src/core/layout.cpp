/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/layout.hpp"
#include "common_fps/constants.hpp"
#include "common_fps/config.hpp"
#include <algorithm>
#include <cmath>

namespace common_fps {

Vec2 compute_anchor(const OverlayConfig& cfg) {
    return compute_layout(cfg, kLogicalWidth, kLogicalHeight).anchor;
}

OverlayLayout compute_layout(const OverlayConfig& input, float scene_width,
                             float scene_height, bool loading) {
    const OverlayConfig cfg = sanitize_config(input);
    if (!std::isfinite(scene_width) || scene_width < 640 || scene_width > 7680)
        scene_width = kLogicalWidth;
    if (!std::isfinite(scene_height) || scene_height < 360 || scene_height > 4320)
        scene_height = kLogicalHeight;
    const float scale = std::min(scene_width / kLogicalWidth, scene_height / kLogicalHeight);
    OverlayLayout result;
    result.font_size = std::max(1, static_cast<int>(std::lround(cfg.font_size * scale)));
    result.label_advance = cfg.show_label ? result.font_size * 2.7f : 0;
    result.width = result.label_advance + result.font_size * (loading ? 4.5f : 2.5f);
    result.height = result.font_size * 1.45f;
    const float left = std::min(cfg.margin_x * scale, scene_width - result.width);
    const float top = std::min(cfg.margin_y * scale, scene_height - result.height);
    const float right = std::max(0.0f, scene_width - result.width - left);
    const float bottom = std::max(0.0f, scene_height - result.height - top);

    switch (cfg.corner) {
        case Corner::TopLeft:
            result.anchor = {left, top}; break;

        case Corner::TopRight:
            result.anchor = {right, top}; break;

        case Corner::BottomRight:
            result.anchor = {right, bottom}; break;

        case Corner::BottomLeft:
        default:
            result.anchor = {left, bottom}; break;
    }
    return result;
}

} // namespace common_fps
