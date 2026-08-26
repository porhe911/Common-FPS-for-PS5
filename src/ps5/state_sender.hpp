/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/wire.hpp"

namespace common_fps::ps5 {

class StateSender {
public:
    StateSender();
    ~StateSender();

    bool open();
    bool send(const WirePacket& packet);

private:
    int socket_ = -1;
};

} // namespace common_fps::ps5
