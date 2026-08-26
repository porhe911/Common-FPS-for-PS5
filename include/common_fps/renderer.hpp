/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "common_fps/types.hpp"

namespace common_fps {

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool initialize() = 0;
    virtual void render(const OverlayFrame& frame) = 0;
};

} // namespace common_fps
