/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
namespace common_fps::ps5 {
// Bounded ShellUI reads and the one-byte FW 11.60 guard transaction only.
int shellui_mdbg_read_exact(pid_t pid, std::uintptr_t address,
                           void* destination, std::size_t size) noexcept;
int shellui_mdbg_write_byte(pid_t pid, std::uintptr_t address,
                           std::uint8_t byte) noexcept;
}
