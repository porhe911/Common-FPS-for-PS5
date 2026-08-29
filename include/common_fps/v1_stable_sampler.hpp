/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include "common_fps/platform.hpp"

#include <cstdint>
#include <optional>

namespace common_fps::v1_stable {

class Sampler {
public:
    explicit Sampler(Platform& platform);

    bool attach(ProcessId pid);
    std::optional<double> sample();
    void reset();

    bool attached() const noexcept;
    ProcessId pid() const noexcept;
    std::uintptr_t counter_address() const noexcept;

private:
    bool resolve_counter_address();

    Platform& platform_;
    ProcessId pid_ = -1;
    std::uintptr_t module_base_ = 0;
    std::uintptr_t counter_address_ = 0;
    bool have_baseline_ = false;
    std::uint32_t previous_counter_ = 0;
    std::uint64_t previous_time_us_ = 0;
};

} // namespace common_fps::v1_stable
