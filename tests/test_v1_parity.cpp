/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/types.hpp"
#include "common_fps/v1_stable_wire.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

using namespace common_fps;

int main() {
    const auto cfg = default_config();

    // Stable v1.0.0 visible UI defaults.
    assert(cfg.corner == Corner::BottomLeft);
    assert(cfg.font_size == 26);

    // Historical v1.0.0 did NOT send an integer on the wire. It retained
    // tenth-FPS precision as a double and the ShellUI renderer formatted the
    // visible value with %.0f.
    v1_stable::FpsPacket packet{};
    v1_stable::set_numeric(packet, 1, 59.6);
    assert(std::fabs(packet.fps - 59.6) < 0.000001);

    char visible[32]{};
    std::snprintf(visible, sizeof(visible), "%.0f", packet.fps);
    assert(std::string(visible) == "60");

    // Loading is a separate raw-text packet in the stable protocol.
    v1_stable::set_loading(packet, 2);
    assert(std::string(packet.raw) == "FPS\tloading\n");

    std::cout << "v1.0.0 visible-display parity defaults: PASS\n";
    return 0;
}
