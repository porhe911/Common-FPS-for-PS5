/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "common_fps/types.hpp"
#include <string>
#include <string_view>

namespace common_fps {

OverlayConfig default_config();
OverlayConfig parse_config_text(std::string_view text);
std::string serialize_config(const OverlayConfig& cfg);
const char* corner_name(Corner corner);

} // namespace common_fps
