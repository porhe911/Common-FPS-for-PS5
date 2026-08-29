#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::legacy_v028b {

// FW 9.60 source reconstruction of the hardware-proven v0.28b DMAP reader.
// No ptrace, PT_IO or MDBG credential switch is used by these functions.
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

} // namespace common_fps::legacy_v028b
