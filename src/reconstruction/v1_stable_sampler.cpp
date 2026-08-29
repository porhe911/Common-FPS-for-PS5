/* Common FPS for PS5 - GPL-3.0-or-later */
#include "common_fps/v1_stable_sampler.hpp"

#include "common_fps/constants.hpp"
#include "common_fps/v1_stable_wire.hpp"

#include <array>
#include <cstring>

namespace common_fps::v1_stable {

Sampler::Sampler(Platform& platform)
    : platform_(platform) {}

bool Sampler::attach(ProcessId pid) {
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

void Sampler::reset() {
    pid_ = -1;
    module_base_ = 0;
    counter_address_ = 0;
    have_baseline_ = false;
    previous_counter_ = 0;
    previous_time_us_ = 0;
}

bool Sampler::attached() const noexcept {
    return pid_ >= 0 && counter_address_ != 0;
}

ProcessId Sampler::pid() const noexcept {
    return pid_;
}

std::uintptr_t Sampler::counter_address() const noexcept {
    return counter_address_;
}

bool Sampler::resolve_counter_address() {
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
                pid_, static_cast<std::uintptr_t>(pointer),
                &root, sizeof(root))) {
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

std::optional<double> Sampler::sample() {
    if (!attached())
        return std::nullopt;

    if (!platform_.process_alive(pid_)) {
        reset();
        return std::nullopt;
    }

    std::uint32_t current_counter = 0;
    if (!platform_.read_memory(
            pid_, counter_address_, &current_counter,
            sizeof(current_counter))) {
        reset();
        return std::nullopt;
    }

    const auto now_us = platform_.monotonic_us();
    if (!have_baseline_) {
        have_baseline_ = true;
        previous_counter_ = current_counter;
        previous_time_us_ = now_us;
        return std::nullopt;
    }

    const auto elapsed_us = now_us - previous_time_us_;
    const auto value = calculate_fps(
        previous_counter_, current_counter, elapsed_us);

    previous_counter_ = current_counter;
    previous_time_us_ = now_us;
    return value;
}

} // namespace common_fps::v1_stable
