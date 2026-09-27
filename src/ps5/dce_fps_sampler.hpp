/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "common_fps/platform.hpp"

#include <cstdint>
#include <optional>

namespace common_fps::ps5 {

/*
 * Firmware-neutral fallback sampler based on the display controller flip
 * counter.  It never reads or writes game process memory.
 */
class DceFpsSampler final {
public:
    explicit DceFpsSampler(Platform& platform) noexcept;
    ~DceFpsSampler();

    DceFpsSampler(const DceFpsSampler&) = delete;
    DceFpsSampler& operator=(const DceFpsSampler&) = delete;

    std::optional<int> sample() noexcept;
    void reset() noexcept;

private:
    bool ensure_open() noexcept;

    Platform& platform_;
    int fd_ = -1;
    bool have_baseline_ = false;
    std::uint64_t previous_flip_count_ = 0;
    std::uint64_t previous_time_us_ = 0;
    unsigned consecutive_failures_ = 0;
    bool backend_logged_ = false;
};

} // namespace common_fps::ps5
