#pragma once

#include <cstdint>
#include <optional>
#include <sys/types.h>

namespace common_fps::legacy_v028b {

[[nodiscard]] std::optional<std::uintptr_t>
resolve_videoout_counter(pid_t pid) noexcept;

[[nodiscard]] bool read_videoout_counter(
    pid_t pid,
    std::uintptr_t counter_address,
    std::uint32_t& value) noexcept;

} // namespace common_fps::legacy_v028b
