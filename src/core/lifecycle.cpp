/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/lifecycle.hpp"
#include "common_fps/layout.hpp"


namespace common_fps {

Lifecycle::Lifecycle(Platform& platform, OverlayConfig cfg)
    : platform_(platform),
      sampler_(platform),
      config_(cfg) {}

void Lifecycle::set_config(const OverlayConfig& cfg) {
    config_ = cfg;
}

void Lifecycle::reset() {
    sampler_.reset();
}

OverlayFrame Lifecycle::tick() {
    OverlayFrame frame;
    frame.visible = true;
    frame.loading = true;
    frame.config = config_;

    // Critical stability rule:
    // recalculate the anchor every update. Never accumulate cursor_x/y.
    frame.anchor = compute_anchor(config_);

    if (!sampler_.attached()) {
        const auto pid = platform_.find_game_process();
        if (!pid)
            return frame;

        if (!sampler_.attach(*pid))
            return frame;
    }

    const auto value = sampler_.sample();
    if (!value)
        return frame;

    frame.loading = false;
    frame.fps = *value;
    return frame;
}

} // namespace common_fps
