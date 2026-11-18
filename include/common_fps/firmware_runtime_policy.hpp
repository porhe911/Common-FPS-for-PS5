/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
namespace common_fps {

// Label the hardware-test path; this is not a renderer/sampler suppression gate.
inline constexpr bool firmware_is_fw1160(std::uint32_t sdk) noexcept {
    return (sdk & 0xffff0000U) == 0x11600000U;
}

// The submitted FW 9.x log reports SDK 0x09050005.
// Keep the tested 9.00 MDBG branch and include only this observed variant.
inline constexpr bool firmware_uses_fw900_mdbg_guard(std::uint32_t sdk) noexcept {
    const std::uint32_t family = sdk & 0xffff0000U;
    return family == 0x09000000U || family == 0x09050000U;
}
} // namespace common_fps
