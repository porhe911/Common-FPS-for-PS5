/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace common_fps {

inline constexpr std::uint32_t kRendererHealthMagic = 0x48504643U; // "CFPH"
inline constexpr std::uint16_t kRendererHealthVersion = 1;
inline constexpr std::uint16_t kRendererHealthPort = 39029;

/*
 * Bound by the injected ShellUI companion for the whole lifetime of the
 * SceShellUI process.  Unlike the heartbeat this is deliberately persistent:
 * if a controller is stopped and started again while the old injected code is
 * still resident, a second injection is refused instead of re-hooking PUI.
 */
inline constexpr std::uint16_t kRendererSentinelPort = 39030;

inline constexpr std::uint16_t kRendererHealthPacketSize = 24;

enum class RendererHealthPhase : std::uint16_t {
    ReceiverReady = 1,
    VisualReady = 2,
};

#pragma pack(push, 1)
struct RendererHealthPacket {
    std::uint32_t magic = kRendererHealthMagic;
    std::uint16_t version = kRendererHealthVersion;
    std::uint16_t size = kRendererHealthPacketSize;
    std::int32_t shellui_pid = -1;
    std::uint16_t phase =
        static_cast<std::uint16_t>(RendererHealthPhase::ReceiverReady);
    std::uint16_t reserved = 0;
    std::uint64_t sequence = 0;
};
#pragma pack(pop)

static_assert(sizeof(RendererHealthPacket) == 24);

[[nodiscard]] inline bool
valid_renderer_health_packet(const RendererHealthPacket& packet) noexcept {
    return
        packet.magic == kRendererHealthMagic &&
        packet.version == kRendererHealthVersion &&
        packet.size == kRendererHealthPacketSize &&
        (packet.phase ==
             static_cast<std::uint16_t>(RendererHealthPhase::ReceiverReady) ||
         packet.phase ==
             static_cast<std::uint16_t>(RendererHealthPhase::VisualReady));
}

} // namespace common_fps
