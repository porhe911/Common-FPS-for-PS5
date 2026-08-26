/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "common_fps/fps_sampler.hpp"
#include "common_fps/types.hpp"

namespace common_fps {

class Lifecycle {
public:
    Lifecycle(Platform& platform, OverlayConfig cfg);

    OverlayFrame tick();
    void set_config(const OverlayConfig& cfg);
    void reset();

private:
    Platform& platform_;
    FpsSampler sampler_;
    OverlayConfig config_;
};

} // namespace common_fps
