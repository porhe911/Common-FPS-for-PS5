/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
namespace common_fps {

// Label the hardware-test path; this is not a renderer/sampler suppression gate.
inline constexpr bool firmware_is_fw1160(std::uint32_t sdk) noexcept {
    return (sdk & 0xffff0000U) == 0x11600000U;
}

} // namespace common_fps
