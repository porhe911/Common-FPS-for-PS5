/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace common_fps {

struct ModuleInfo {
    std::uintptr_t base = 0;
    std::string name;
};

class Platform {
public:
    virtual ~Platform() = default;

    virtual std::optional<ProcessId> find_game_process() = 0;
    virtual bool process_alive(ProcessId pid) = 0;

    virtual std::optional<ModuleInfo>
    find_module(ProcessId pid, const char* module_name) = 0;

    /*
     * Read-only remote process memory.
     * Common FPS never writes or patches game memory.
     */
    virtual bool read_memory(
        ProcessId pid,
        std::uintptr_t address,
        void* out,
        std::size_t size) = 0;

    /* Monotonic time used to calculate a counter delta into FPS. */
    virtual std::uint64_t monotonic_us() = 0;

    virtual void sleep_ms(unsigned milliseconds) = 0;
};

} // namespace common_fps
