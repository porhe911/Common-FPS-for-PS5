/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
namespace common_fps::ps5 {
// Used only for the bounded FW 11.60 ShellUI code-read fallback.
int shellui_mdbg_read_exact(pid_t pid, std::uintptr_t address,
                           void* destination, std::size_t size) noexcept;
}
