/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/platform.hpp"

#include <cstdint>
#include <optional>

namespace common_fps {

class FpsSampler {
public:
    explicit FpsSampler(Platform& platform);

    bool attach(ProcessId pid);
    void reset();

    [[nodiscard]] bool attached() const noexcept;
    [[nodiscard]] ProcessId pid() const noexcept;
    [[nodiscard]] std::uintptr_t counter_address() const noexcept;

    /*
     * Common FPS public contract:
     *
     *     FPS: 59
     *
     * Never returns/display decimal FPS.
     * Internally the counter is calculated in tenths and rounded once.
     */
    std::optional<int> sample();

private:
    bool resolve_counter_address();

    Platform& platform_;
    ProcessId pid_ = -1;
    std::uintptr_t module_base_ = 0;
    std::uintptr_t counter_address_ = 0;

    /* 0=need baseline, 1=discard first delta, 2=normal sampling. */
    std::uint8_t warmup_stage_ = 0;
    std::uint32_t previous_counter_ = 0;
    std::uint64_t previous_time_us_ = 0;
};

} // namespace common_fps
