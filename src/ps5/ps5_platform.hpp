/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/platform.hpp"

#include <cstdint>

namespace common_fps::ps5 {

class Ps5Platform final : public Platform {
public:
    std::optional<ProcessId> find_game_process() override;
    bool process_alive(ProcessId pid) override;

    bool begin_process_inspection(ProcessId pid) override;
    void end_process_inspection(ProcessId pid) override;

    std::optional<ModuleInfo>
    find_module(ProcessId pid, const char* module_name) override;

    bool read_memory(
        ProcessId pid,
        std::uintptr_t address,
        void* out,
        std::size_t size) override;

    std::uint64_t monotonic_us() override;
    void sleep_ms(unsigned milliseconds) override;

private:
    std::uintptr_t saved_authid_ = 0;
    bool inspection_active_ = false;
};

} // namespace common_fps::ps5
