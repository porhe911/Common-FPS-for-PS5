#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::legacy_v028b {

struct InjectionResult {
    bool attached = false;
    bool elf_loaded = false;
    bool payload_args_ready = false;
    bool bootstrap_started = false;
    bool pthread_create_ok = false;
    int pthread_create_rc = -1;
};

// One-time SceShellUI renderer injection. This is deliberately separate from
// the FPS lifecycle and must never be called once per game.
[[nodiscard]] InjectionResult inject_renderer_once(
    pid_t shellui_pid,
    const std::uint8_t* renderer_elf,
    std::size_t renderer_size) noexcept;

} // namespace common_fps::legacy_v028b
