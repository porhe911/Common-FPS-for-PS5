#pragma once

#include <cstdint>
#include <sys/types.h>

namespace common_fps::legacy_v028b {

// Call a target function while the process is already ptrace-attached. The
// target function must execute INT3 before returning. Registers are restored
// before this function returns.
[[nodiscard]] long call_until_breakpoint(
    pid_t pid,
    std::uintptr_t address,
    std::uint64_t arg0 = 0,
    std::uint64_t arg1 = 0,
    std::uint64_t arg2 = 0,
    std::uint64_t arg3 = 0,
    std::uint64_t arg4 = 0,
    std::uint64_t arg5 = 0) noexcept;

} // namespace common_fps::legacy_v028b
