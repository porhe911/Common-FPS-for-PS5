/*
 * Common FPS for PS5
 * Compatibility behavior reconstructed from PHU Games Tools v1.14.25-r11.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace common_fps::v1_stable {

inline constexpr std::size_t kAbsoluteJumpSize = 14;
inline constexpr std::size_t kDetourProbeBytes = 32;

using JumpBytes = std::array<std::uint8_t, kAbsoluteJumpSize>;

/*
 * Direct-patch form used when sceKernelMprotect succeeds:
 *   FF 25 00 00 00 00 <destination: uint64 little-endian>
 */
JumpBytes make_ff25_jump(std::uint64_t destination) noexcept;

/*
 * DMAP fallback form used by the PHU r11 donor:
 *   49 BB <destination: uint64 little-endian> 41 FF E3 90
 *   mov r11, imm64 ; jmp r11 ; nop
 */
JumpBytes make_r11_jump(std::uint64_t destination) noexcept;

/* Detect the etaHEN/PHU-style 14-byte absolute FF25 jump. */
bool is_ff25_absolute_jump(const std::uint8_t* bytes,
                           std::size_t size) noexcept;

/* Decode the destination from a validated FF25 absolute jump. */
std::uint64_t ff25_destination(const std::uint8_t* bytes,
                               std::size_t size) noexcept;

} // namespace common_fps::v1_stable
