/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/fps_sampler.hpp"
#include "common_fps/constants.hpp"

#include <array>
#include <cstring>

namespace common_fps {

FpsSampler::FpsSampler(Platform& platform)
    : platform_(platform) {}

bool FpsSampler::attach(ProcessId pid) {
    reset();

    const auto module = platform_.find_module(pid, "libSceVideoOut.sprx");
    if (!module || module->base == 0)
        return false;

    pid_ = pid;
    module_base_ = module->base;

    if (!resolve_counter_address()) {
        reset();
        return false;
    }

    return true;
}

void FpsSampler::reset() {
    pid_ = -1;
    module_base_ = 0;
    counter_address_ = 0;
    have_baseline_ = false;
    previous_counter_ = 0;
    previous_time_us_ = 0;
}

bool FpsSampler::attached() const noexcept {
    return pid_ >= 0 && counter_address_ != 0;
}

ProcessId FpsSampler::pid() const noexcept {
    return pid_;
}

std::uintptr_t FpsSampler::counter_address() const noexcept {
    return counter_address_;
}

bool FpsSampler::resolve_counter_address() {
    constexpr std::size_t kTableSize =
        kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;

    std::array<std::uint8_t, kTableSize> table{};

    if (!platform_.read_memory(
            pid_,
            module_base_ + kVideoOutProbeTableOffset,
            table.data(),
            table.size())) {
        return false;
    }

    for (std::size_t i = 0; i < kVideoOutProbeEntryCount; ++i) {
        const auto* entry = table.data() + i * kVideoOutProbeEntrySize;

        std::uint32_t enabled = 0;
        std::uint64_t pointer = 0;

        std::memcpy(&enabled, entry + 0x00, sizeof(enabled));
        std::memcpy(&pointer, entry + 0x08, sizeof(pointer));

        if (enabled == 0 || pointer == 0)
            continue;

        std::uint64_t root = 0;

        if (!platform_.read_memory(
                pid_,
                static_cast<std::uintptr_t>(pointer),
                &root,
                sizeof(root))) {
            return false;
        }

        if (root == 0)
            return false;

        counter_address_ =
            static_cast<std::uintptr_t>(root) + kVideoOutCounterOffset;
        return true;
    }

    return false;
}

std::optional<int> FpsSampler::sample() {
    if (!attached())
        return std::nullopt;

    if (!platform_.process_alive(pid_)) {
        reset();
        return std::nullopt;
    }

    std::uint32_t current_counter = 0;

    if (!platform_.read_memory(
            pid_,
            counter_address_,
            &current_counter,
            sizeof(current_counter))) {
        reset();
        return std::nullopt;
    }

    const std::uint64_t now_us = platform_.monotonic_us();

    if (!have_baseline_) {
        have_baseline_ = true;
        previous_counter_ = current_counter;
        previous_time_us_ = now_us;
        return std::nullopt;
    }

    const std::uint64_t elapsed_us = now_us - previous_time_us_;
    if (elapsed_us == 0)
        return std::nullopt;

    const std::uint32_t delta =
        static_cast<std::uint32_t>(
            current_counter - previous_counter_);

    /*
     * Reconstruct stable v1.0.0 intermediate value in tenths:
     *
     *     tenths = delta * 10,000,000 / elapsed_us
     *
     * Example:
     *     596 -> 59.6 FPS internally.
     *
     * But Common FPS intentionally exposes ONLY integer FPS.
     */
    const std::uint64_t scaled =
        static_cast<std::uint64_t>(delta) * 10'000'000ULL;

    const std::uint32_t tenths =
        static_cast<std::uint32_t>(scaled / elapsed_us);

    previous_counter_ = current_counter;
    previous_time_us_ = now_us;

    if (tenths > kMaxTenthsFps)
        return std::nullopt;

    /*
     * Round to nearest integer without floating point:
     *
     * 59.4 -> 59
     * 59.5 -> 60
     * 59.6 -> 60
     */
    const int integer_fps =
        static_cast<int>((tenths + 5U) / 10U);

    return integer_fps;
}

} // namespace common_fps
