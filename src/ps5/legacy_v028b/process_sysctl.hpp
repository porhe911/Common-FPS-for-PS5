#pragma once

#include <sys/types.h>

namespace common_fps::legacy_v028b {

// Returns the last non-self process whose p_comm matches name, or -1.
// Uses the same KERN_PROC record layout as the stable v0.28b producer.
[[nodiscard]] pid_t find_process_pid_sysctl(const char* name) noexcept;

// Convenience wrapper for the foreground game process.
[[nodiscard]] pid_t find_game_pid_sysctl() noexcept;

} // namespace common_fps::legacy_v028b
