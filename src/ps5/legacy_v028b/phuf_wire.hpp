#pragma once

#include <cstddef>
#include <cstdint>

namespace common_fps::legacy_v028b {

constexpr std::uint32_t kPhufMagic = 0x46554850U; // "PHUF" in memory
constexpr std::uint32_t kPhufVersion = 1U;
constexpr std::uint16_t kPhufUdpPort = 55541U;
constexpr std::size_t kPhufTextSize = 1024U;

struct PhufStatePacket {
    std::uint32_t magic = kPhufMagic;
    std::uint32_t version = kPhufVersion;
    std::uint64_t sequence = 0;
    double fps = 0.0;
    std::uint64_t reserved = 0;
    char text[kPhufTextSize]{};
};

static_assert(offsetof(PhufStatePacket, sequence) == 0x08);
static_assert(offsetof(PhufStatePacket, fps) == 0x10);
static_assert(offsetof(PhufStatePacket, text) == 0x20);
static_assert(sizeof(PhufStatePacket) == 0x420);

} // namespace common_fps::legacy_v028b
