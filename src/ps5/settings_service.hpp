/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "common_fps/settings.hpp"
#include <pthread.h>

namespace common_fps::ps5 {
// Lifetime is the resident controller's lifetime; no detached shutdown actions.
bool start_settings_service(SettingsStore& store);
}
