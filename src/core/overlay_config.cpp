/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/config.hpp"
#include <algorithm>
#include <cmath>

namespace common_fps {
OverlayConfig sanitize_config(OverlayConfig cfg) {
    if (cfg.corner != Corner::TopLeft && cfg.corner != Corner::TopRight &&
        cfg.corner != Corner::BottomLeft && cfg.corner != Corner::BottomRight)
        cfg.corner = Corner::BottomLeft;
    cfg.font_size = std::clamp(cfg.font_size, kMinFontSize, kMaxFontSize);
    cfg.margin_x = std::isfinite(cfg.margin_x)
        ? std::clamp(cfg.margin_x, 0.0f, 240.0f) : kDefaultMarginX;
    cfg.margin_y = std::isfinite(cfg.margin_y)
        ? std::clamp(cfg.margin_y, 0.0f, 240.0f) : kDefaultMarginY;
    cfg.label_color &= 0xFFFFFF;
    cfg.value_color &= 0xFFFFFF;
    return cfg;
}

bool same_config(const OverlayConfig& a, const OverlayConfig& b) {
    return a.corner == b.corner && a.font_size == b.font_size &&
        a.margin_x == b.margin_x && a.margin_y == b.margin_y &&
        a.show_label == b.show_label && a.label_color == b.label_color &&
        a.value_color == b.value_color;
}

} // namespace common_fps
