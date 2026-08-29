/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace common_fps::v1_stable {

inline constexpr std::uint32_t kWireMagic = 0x46554850u; // PHUF
inline constexpr std::uint32_t kWireVersion = 1;
inline constexpr std::uint16_t kWirePort = 55541;
inline constexpr std::size_t kTextCapacity = 1024;
inline constexpr std::uint32_t kMaxTenthsFps = 3000;

/*
 * Exact field names/layout recovered from PHU r11 DWARF and verified against
 * the hardware-stable Common FPS v1.0.0 sender/receiver disassembly.
 */
struct FpsPacket {
    std::uint32_t magic = kWireMagic;
    std::uint32_t version = kWireVersion;
    std::uint64_t sequence = 0;
    double fps = 0.0;
    std::uint64_t last_ns = 0;
    char text[kTextCapacity]{};
};

static_assert(offsetof(FpsPacket, magic) == 0x00);
static_assert(offsetof(FpsPacket, version) == 0x04);
static_assert(offsetof(FpsPacket, sequence) == 0x08);
static_assert(offsetof(FpsPacket, fps) == 0x10);
static_assert(offsetof(FpsPacket, last_ns) == 0x18);
static_assert(offsetof(FpsPacket, text) == 0x20);
static_assert(sizeof(FpsPacket) == 0x420);

inline void set_loading(FpsPacket& packet, std::uint64_t sequence) {
    packet = {};
    packet.magic = kWireMagic;
    packet.version = kWireVersion;
    packet.sequence = sequence;
    constexpr char kLoading[] = "FPS\tloading\n";
    std::memcpy(packet.text, kLoading, sizeof(kLoading));
}

inline void set_numeric(FpsPacket& packet,
                        std::uint64_t sequence,
                        double fps) {
    packet = {};
    packet.magic = kWireMagic;
    packet.version = kWireVersion;
    packet.sequence = sequence;
    packet.fps = fps;
    packet.text[0] = '\0';
}

inline std::optional<double> calculate_fps(std::uint32_t previous_counter,
                                           std::uint32_t current_counter,
                                           std::uint64_t elapsed_us) {
    if (elapsed_us == 0)
        return std::nullopt;

    const std::uint32_t delta = current_counter - previous_counter;
    const std::uint64_t tenths =
        (static_cast<std::uint64_t>(delta) * 10000000ull) / elapsed_us;

    if (tenths > kMaxTenthsFps)
        return std::nullopt;

    return static_cast<double>(tenths) / 10.0;
}

} // namespace common_fps::v1_stable
