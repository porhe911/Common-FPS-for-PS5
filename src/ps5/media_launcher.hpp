/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>

namespace common_fps::ps5 {
// Called on the controller thread, before samplers or the HTTP worker start.
// False only if original credentials could not be restored.
bool ensure_media_launcher(std::uint32_t sdk_version);
}
