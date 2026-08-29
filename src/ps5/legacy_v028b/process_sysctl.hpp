#pragma once

#include <sys/types.h>

namespace common_fps::legacy_v028b {

// Returns the first foreground game process named eboot.bin, or -1 when no
// game is present. This mirrors the stable producer's userland KERN_PROC path.
[[nodiscard]] pid_t find_game_pid_sysctl() noexcept;

} // namespace common_fps::legacy_v028b
