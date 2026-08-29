/*
 * Common FPS for PS5
 *
 * Compatibility ABI reconstructed from PHU Games Tools v1.14.25-r11.
 * PHU Games Tools is distributed as mixed GPL/MIT; this project is
 * GPL-3.0-or-later and preserves PHU provenance for derived compatibility code.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace common_fps::phu_compat {

/* Bytes in memory are "PIUH" on little-endian x86-64. */
inline constexpr std::uint32_t kHookIpcMagic = 0x48554950u;
inline constexpr std::size_t kHookIpcMapSize = 0x1000;
inline constexpr std::size_t kHookIpcDataSize = 512;
inline constexpr const char* kHookIpcPath = "/system_tmp/phu_hook_ipc";

enum class HookIpcState : std::uint32_t {
    Idle = 0,
    Request = 1,
    Done = 2,
    Error = 3,
};

enum class HookIpcOp : std::uint32_t {
    Read = 1,
    Write = 2,
};

/*
 * Exact 544-byte shared-memory layout recovered from donor DWARF and verified
 * against DetourFunction machine code.
 */
struct HookIpc {
    volatile std::uint32_t magic;       // +0x00
    volatile std::uint32_t state;       // +0x04
    volatile std::uint32_t op;          // +0x08
    volatile std::uint32_t prot;        // +0x0c
    volatile std::uint64_t addr;        // +0x10
    volatile std::uint32_t len;         // +0x18
    volatile std::int32_t error_code;   // +0x1c
    volatile std::uint8_t data[kHookIpcDataSize]; // +0x20
};

static_assert(offsetof(HookIpc, magic) == 0x00);
static_assert(offsetof(HookIpc, state) == 0x04);
static_assert(offsetof(HookIpc, op) == 0x08);
static_assert(offsetof(HookIpc, prot) == 0x0c);
static_assert(offsetof(HookIpc, addr) == 0x10);
static_assert(offsetof(HookIpc, len) == 0x18);
static_assert(offsetof(HookIpc, error_code) == 0x1c);
static_assert(offsetof(HookIpc, data) == 0x20);
static_assert(sizeof(HookIpc) == 544);

} // namespace common_fps::phu_compat
