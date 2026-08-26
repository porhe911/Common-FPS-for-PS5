/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/layout.hpp"
#include "common_fps/constants.hpp"

namespace common_fps {

Vec2 compute_anchor(const OverlayConfig& cfg) {
    // Approximate single-line logical bounding box.
    const float width = static_cast<float>(cfg.font_size) * 5.2f;
    const float height = static_cast<float>(cfg.font_size) * 1.45f;

    switch (cfg.corner) {
        case Corner::TopLeft:
            return {cfg.margin_x, cfg.margin_y};

        case Corner::TopRight:
            return {kLogicalWidth - width - cfg.margin_x, cfg.margin_y};

        case Corner::BottomRight:
            return {
                kLogicalWidth - width - cfg.margin_x,
                kLogicalHeight - height - cfg.margin_y
            };

        case Corner::BottomLeft:
        default:
            return {
                cfg.margin_x,
                kLogicalHeight - height - cfg.margin_y
            };
    }
}

} // namespace common_fps
