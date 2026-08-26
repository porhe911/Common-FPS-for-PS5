/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/types.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>

using namespace common_fps;

int main() {
    const auto cfg = default_config();

    // Stable v1.0.0 UI defaults.
    assert(cfg.corner == Corner::BottomLeft);
    assert(cfg.font_size == 26);

    // Integer-only public transport.
    OverlayFrame frame;
    frame.loading = false;
    frame.fps = 59;
    static_assert(std::is_same_v<decltype(frame.fps), int>);

    // Ensure our source model never needs a decimal string.
    const std::string display =
        std::string("FPS: ") + std::to_string(frame.fps);

    assert(display == "FPS: 59");
    assert(display.find('.') == std::string::npos);

    std::cout << "v1.0.0 parity defaults: PASS\n";
    return 0;
}
