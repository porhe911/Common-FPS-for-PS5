/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/wire.hpp"
#include "common_fps/constants.hpp"
#include "common_fps/layout.hpp"

#include <algorithm>

namespace common_fps {

static std::uint8_t encode_corner(Corner corner) {
    switch (corner) {
        case Corner::TopLeft: return 0;
        case Corner::TopRight: return 1;
        case Corner::BottomLeft: return 2;
        case Corner::BottomRight: return 3;
    }
    return 2;
}

static Corner decode_corner(std::uint8_t corner) {
    switch (corner) {
        case 0: return Corner::TopLeft;
        case 1: return Corner::TopRight;
        case 3: return Corner::BottomRight;
        case 2:
        default: return Corner::BottomLeft;
    }
}

WirePacket make_wire_packet(
    const OverlayFrame& frame,
    std::uint64_t sequence) {

    WirePacket packet;
    packet.sequence = sequence;
    packet.fps = frame.fps;
    packet.loading = frame.loading ? 1 : 0;
    packet.corner = encode_corner(frame.config.corner);
    packet.font_size = static_cast<std::uint16_t>(
        std::clamp(frame.config.font_size, kMinFontSize, kMaxFontSize));
    packet.margin_x = frame.config.margin_x;
    packet.margin_y = frame.config.margin_y;
    return packet;
}

std::optional<OverlayFrame>
decode_wire_packet(const WirePacket& packet) {
    if (packet.magic != kWireMagic ||
        packet.version != kWireVersion ||
        packet.size != sizeof(WirePacket)) {
        return std::nullopt;
    }

    OverlayFrame frame;
    frame.visible = true;
    frame.loading = packet.loading != 0;
    frame.fps = packet.fps;
    frame.config.corner = decode_corner(packet.corner);
    frame.config.font_size = std::clamp(
        static_cast<int>(packet.font_size),
        kMinFontSize,
        kMaxFontSize);
    frame.config.margin_x = std::max(0.0f, packet.margin_x);
    frame.config.margin_y = std::max(0.0f, packet.margin_y);
    frame.anchor = compute_anchor(frame.config);
    return frame;
}

} // namespace common_fps
