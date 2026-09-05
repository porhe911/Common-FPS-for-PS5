/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5::stable_sampler {

/*
 * FW 9.60 DMAP reader reconstructed from the hardware-proven sampler.
 * It performs read-only game access and never attaches through ptrace/MDBG.
 */
[[nodiscard]] bool proc_read(
    pid_t pid,
    std::uintptr_t remote,
    void* local,
    std::size_t size) noexcept;

[[nodiscard]] std::uintptr_t translate(
    pid_t pid,
    std::uintptr_t remote,
    std::size_t* page_size) noexcept;

[[nodiscard]] std::uintptr_t dmap_base() noexcept;
[[nodiscard]] std::uint32_t firmware_sdk_version() noexcept;
[[nodiscard]] bool firmware_is_960() noexcept;

} // namespace common_fps::ps5::stable_sampler
