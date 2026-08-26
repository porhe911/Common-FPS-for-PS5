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

/*
 * Stable v1.0.0 VideoOut probe chain (FW 9.60 tested binary):
 *
 *   libSceVideoOut.sprx base
 *       + 0x34980
 *       -> 7 records, 0x18 bytes each
 *       -> first record with non-zero flag and pointer
 *       -> read uint64_t from record.pointer
 *       -> + 0x768
 *       -> uint32_t frame/vsync counter
 *
 * FPS is NOT stored directly as a float/double at +0x768.
 */
inline constexpr std::uintptr_t kVideoOutProbeTableOffset = 0x34980;
inline constexpr std::size_t kVideoOutProbeEntryCount = 7;
inline constexpr std::size_t kVideoOutProbeEntrySize = 0x18;
inline constexpr std::uintptr_t kVideoOutCounterOffset = 0x768;

inline constexpr float kLogicalWidth = 1920.0f;
inline constexpr float kLogicalHeight = 1080.0f;

inline constexpr int kDefaultFontSize = 26;
inline constexpr int kMinFontSize = 18;
inline constexpr int kMaxFontSize = 36;

/* Matches the stable binary's sanity bound: 3000 tenths = 300.0 FPS. */
inline constexpr std::uint32_t kMaxTenthsFps = 3000;

} // namespace common_fps
