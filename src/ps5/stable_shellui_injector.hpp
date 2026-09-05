/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

enum class StableInjectionMode : std::uint8_t {
    StartRendererThread,
    ExerciseStagerWithoutThread,
};

inline constexpr int kPthreadCreateSkippedRc = -778;

struct StableInjectionResult {
    bool auth_changed = false;
    bool auth_restored = false;
    bool attached = false;
    bool elf_loaded = false;
    bool payload_args_ready = false;
    bool remote_stager_ready = false;
    bool remote_functions_ready = false;
    bool target_stack_preserved = false;
    bool bootstrap_started = false;
    bool thread_start_requested = false;
    bool pthread_create_ok = false;
    bool detached = false;
    bool trace_continue_seen = false;
    bool trace_stop_seen = false;
    int pthread_create_rc = -1;
    unsigned imports_resolved = 0;
    unsigned imports_unresolved = 0;
    const char* first_unresolved = nullptr;
};

/*
 * Reconstruct the hardware-proven v0.28b loader contract:
 *
 *   debugger auth -> one PT_ATTACH -> ELF map -> full payload args
 *   -> remote pthread_create -> PT_DETACH(0) -> restore original auth
 *
 * The controller never replaces a live ShellUI thread with the payload entry.
 */
[[nodiscard]] StableInjectionResult inject_shellui_stable(
    pid_t shellui_pid,
    const std::uint8_t* renderer_elf,
    std::size_t renderer_size,
    StableInjectionMode mode =
        StableInjectionMode::StartRendererThread) noexcept;

} // namespace common_fps::ps5
