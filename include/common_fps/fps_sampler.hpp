/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/constants.hpp"
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
     *
     * Internally the stable v1.0.0 algorithm calculates tenths of FPS from
     * the VideoOut counter, then rounds once to the nearest integer.
     */
    std::optional<int> sample();

private:
    bool resolve_counter_address();

    Platform& platform_;
    ProcessId pid_ = -1;
    std::uintptr_t module_base_ = 0;
    std::uintptr_t counter_address_ = 0;

    bool have_baseline_ = false;
    unsigned warmup_deltas_remaining_ = kWarmupDeltasToDiscard;
    std::uint32_t previous_counter_ = 0;
    std::uint64_t previous_time_us_ = 0;
};

} // namespace common_fps
