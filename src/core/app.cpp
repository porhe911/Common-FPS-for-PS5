/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/app.hpp"
#include "common_fps/lifecycle.hpp"

namespace common_fps {

int run(Platform& platform, Renderer& renderer, const OverlayConfig& cfg) {
    if (!renderer.initialize())
        return -1;

    Lifecycle lifecycle(platform, cfg);

    for (;;) {
        renderer.render(lifecycle.tick());
        platform.sleep_ms(1000);
    }
}

} // namespace common_fps
