/*
 * Common FPS for PS5
 * Minimal target-process dynlib ABI used by stable-v1 reconstruction.
 * Layout mirrors the public etaHEN fps_elf module_info_t definition without
 * pulling etaHEN's kernel structure headers into the sysctl translation unit.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace common_fps::ps5 {

inline constexpr long kSysDlGetList = 0x217;
inline constexpr long kSysDlGetInfo2 = 0x2cd;

inline constexpr std::size_t kDynlibNameLength = 128;
inline constexpr std::size_t kDynlibSandboxPathLength = 1024;
inline constexpr std::size_t kDynlibMaxSections = 4;
inline constexpr std::size_t kDynlibFingerprintLength = 20;

struct DynlibModuleSection {
    std::uint64_t vaddr;
    std::uint32_t size;
    std::uint32_t prot;
};

struct DynlibModuleInfo {
    char filename[kDynlibNameLength];
    std::uint64_t handle;
    std::uint8_t unknown0[32];
    std::uint64_t init;
    std::uint64_t fini;
    std::uint64_t eh_frame_hdr;
    std::uint64_t eh_frame_hdr_sz;
    std::uint64_t eh_frame;
    std::uint64_t eh_frame_sz;
    DynlibModuleSection sections[kDynlibMaxSections];
    std::uint8_t unknown7[1176];
    std::uint8_t fingerprint[kDynlibFingerprintLength];
    std::uint32_t unknown8;
    char libname[kDynlibNameLength];
    std::uint32_t unknown9;
    char sandboxed_path[kDynlibSandboxPathLength];
    std::uint64_t sdk_version;
};

} // namespace common_fps::ps5
