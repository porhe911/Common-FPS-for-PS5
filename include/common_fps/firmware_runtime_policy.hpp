/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
namespace common_fps {

// A hardware KP report requires isolation before re-enabling this full path.
inline constexpr bool firmware_requires_preflight(std::uint32_t sdk) noexcept {
    return (sdk & 0xffff0000U) == 0x11600000U;
}

} // namespace common_fps
