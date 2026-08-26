/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cstdint>
#include <string>

namespace common_fps {

using ProcessId = int;

enum class Corner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct Vec2 {
    float x{};
    float y{};
};

struct OverlayConfig {
    Corner corner = Corner::BottomLeft;
    int font_size = 26;
    float margin_x = 10.0f;
    float margin_y = 10.0f;
};

struct OverlayFrame {
    bool visible = false;
    bool loading = true;
    int fps = 0;
    OverlayConfig config{};
    Vec2 anchor{};
};

} // namespace common_fps
