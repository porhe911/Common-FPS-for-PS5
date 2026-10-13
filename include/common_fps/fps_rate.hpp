/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "common_fps/constants.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace common_fps {

/* Allow small polling jitter above 120 Hz, reject bursts, never display >120. */
inline std::optional<int> validated_fps(double fps) noexcept {
    if (!std::isfinite(fps) || fps < 1.0 ||
        fps > static_cast<double>(kMaxTenthsFps) / 10.0)
        return std::nullopt;
    return std::min(static_cast<int>(fps + 0.5), kMaxDisplayFps);
}

} // namespace common_fps
