/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "common_fps/platform.hpp"
#include "common_fps/dce_counter_sampler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace common_fps::ps5 {

/*
 * Firmware-neutral fallback sampler.
 *
 * Priority:
 *   1) HEN shared FPS sample when one exists;
 *   2) /dev/dce fixed flip-counter layout;
 *   3) adaptive DCE 64-bit counter discovery inside the returned 0x60 block.
 *
 * No game-process memory is read or written by this class.
 */
class DceFpsSampler final {
public:
    explicit DceFpsSampler(Platform& platform) noexcept;
    ~DceFpsSampler();

    DceFpsSampler(const DceFpsSampler&) = delete;
    DceFpsSampler& operator=(const DceFpsSampler&) = delete;

    std::optional<int> sample() noexcept;
    void reset() noexcept;
    [[nodiscard]] const char* backend_name() const noexcept;

private:
    bool ensure_open() noexcept;
    std::optional<int> sample_shared_hen() noexcept;
    bool query_dce(std::array<std::uint64_t, 12>& words) noexcept;
    Platform& platform_;
    int fd_ = -1;

    DceCounterSampler counter_sampler_;

    unsigned consecutive_failures_ = 0;
    bool backend_logged_ = false;
    const char* backend_name_ = "none";
};

} // namespace common_fps::ps5
