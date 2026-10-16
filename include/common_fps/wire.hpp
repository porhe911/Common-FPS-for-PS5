/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/types.hpp"

#include <cstdint>
#include <optional>

namespace common_fps {

inline constexpr std::uint32_t kWireMagic = 0x53465043; // "CFPS"
inline constexpr std::uint32_t kWireShutdownMagic = 0x58465043; // "CFPX"
inline constexpr std::uint16_t kWireVersion = 2;
inline constexpr std::uint16_t kDefaultIpcPort = 39028;

#pragma pack(push, 1)
struct WirePacket {
    std::uint32_t magic = kWireMagic;
    std::uint16_t version = kWireVersion;
    std::uint16_t size = sizeof(WirePacket);

    std::uint64_t sequence = 0;

    std::int32_t fps = 0;
    std::uint8_t loading = 1;
    std::uint8_t corner = 2; // BottomLeft
    std::uint16_t font_size = kDefaultFontSize;

    float margin_x = kDefaultMarginX;
    float margin_y = kDefaultMarginY;
    std::uint8_t show_label = 1;
    std::uint8_t reserved[3]{};
    std::uint32_t label_color = 0xB366FF;
    std::uint32_t value_color = 0xFFFFFF;
};
#pragma pack(pop)
static_assert(sizeof(WirePacket) == 44);

WirePacket make_wire_packet(
    const OverlayFrame& frame,
    std::uint64_t sequence);

/* Reserved controller-to-ShellUI quiesce packet; v1.2.0 does not send it. */
WirePacket make_shutdown_wire_packet(std::uint64_t sequence);

[[nodiscard]] bool
is_shutdown_wire_packet(const WirePacket& packet) noexcept;

std::optional<OverlayFrame>
decode_wire_packet(const WirePacket& packet);

} // namespace common_fps
