/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/fps_sampler.hpp"
#include "common_fps/constants.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>

namespace common_fps {
namespace {

#if defined(PS5)
void sampler_log(const char* fmt, ...) {
    FILE* fp = std::fopen(
        "/data/CommonFPS_universal_stage8.log",
        "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}
#else
#define sampler_log(...) ((void)0)
#endif

bool plausible_user_pointer(std::uint64_t value) noexcept {
    return value >= 0x10000ULL &&
        value < 0x0000800000000000ULL &&
        (value & 0x7ULL) == 0;
}

unsigned unsigned_distance(unsigned left, unsigned right) noexcept {
    return left > right ? left - right : right - left;
}

} // namespace

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
    warmup_deltas_remaining_ = kWarmupDeltasToDiscard;
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

    std::array<std::uint8_t, kTableSize> fixed_table{};

    /*
     * Keep the hardware-proven FW 9.60 fast path byte-for-byte compatible.
     * It avoids any discovery delay on the firmware where +0x34980 is known.
     */
    if (platform_.read_memory(
            pid_,
            module_base_ + kVideoOutProbeTableOffset,
            fixed_table.data(),
            fixed_table.size())) {
        for (std::size_t i = 0; i < kVideoOutProbeEntryCount; ++i) {
            const auto* entry =
                fixed_table.data() + i * kVideoOutProbeEntrySize;

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
                    sizeof(root)) ||
                root == 0) {
                continue;
            }

            counter_address_ =
                static_cast<std::uintptr_t>(root) +
                kVideoOutCounterOffset;
            sampler_log(
                "Sampler fixed chain selected pid=%d table=0x%llx "
                "record=%zu pointer=0x%llx root=0x%llx "
                "counter=0x%llx",
                pid_,
                static_cast<unsigned long long>(
                    module_base_ + kVideoOutProbeTableOffset),
                i,
                static_cast<unsigned long long>(pointer),
                static_cast<unsigned long long>(root),
                static_cast<unsigned long long>(counter_address_));
            return true;
        }
    }

    /*
     * Firmware-independent fallback.  Scan only the small writable-data
     * neighbourhood used by libSceVideoOut, never executable game code.  A
     * structural match is not trusted until its counter advances at a stable
     * display-like rate in two independent 250 ms windows.
     */
    constexpr std::uintptr_t kScanBegin = 0x18000;
    constexpr std::uintptr_t kScanEnd = 0x70000;
    constexpr std::size_t kScanStep = 0x2000;
    constexpr std::size_t kScanReadSize = kScanStep + kTableSize;
    constexpr std::size_t kMaxCandidates = 64;

    struct Candidate {
        std::uintptr_t table = 0;
        std::size_t record = 0;
        std::uint64_t pointer = 0;
        std::uint64_t root = 0;
        std::uintptr_t counter = 0;
        std::uint32_t first = 0;
        std::uint32_t second = 0;
        std::uint32_t third = 0;
        bool second_ok = false;
        bool third_ok = false;
    };

    std::array<Candidate, kMaxCandidates> candidates{};
    std::size_t candidate_count = 0;
    std::array<std::uint8_t, kScanReadSize> window{};

    auto add_candidate = [&](std::uintptr_t table_address,
                             std::size_t record_index,
                             std::uint64_t pointer) {
        if (candidate_count == candidates.size() ||
            !plausible_user_pointer(pointer)) {
            return;
        }

        std::uint64_t root = 0;
        if (!platform_.read_memory(
                pid_,
                static_cast<std::uintptr_t>(pointer),
                &root,
                sizeof(root)) ||
            !plausible_user_pointer(root)) {
            return;
        }

        const std::uintptr_t counter =
            static_cast<std::uintptr_t>(root) +
            kVideoOutCounterOffset;

        for (std::size_t i = 0; i < candidate_count; ++i) {
            if (candidates[i].counter == counter)
                return;
        }

        std::uint32_t first = 0;
        if (!platform_.read_memory(
                pid_,
                counter,
                &first,
                sizeof(first))) {
            return;
        }

        Candidate& candidate = candidates[candidate_count++];
        candidate.table = table_address;
        candidate.record = record_index;
        candidate.pointer = pointer;
        candidate.root = root;
        candidate.counter = counter;
        candidate.first = first;
    };

    for (std::uintptr_t offset = kScanBegin;
         offset < kScanEnd && candidate_count < kMaxCandidates;
         offset += kScanStep) {
        const std::uintptr_t address = module_base_ + offset;
        if (!platform_.read_memory(
                pid_,
                address,
                window.data(),
                window.size())) {
            continue;
        }

        for (std::size_t local = 0;
             local + kTableSize <= window.size() &&
             candidate_count < kMaxCandidates;
             local += sizeof(std::uint64_t)) {
            const auto* table = window.data() + local;

            for (std::size_t record = 0;
                 record < kVideoOutProbeEntryCount;
                 ++record) {
                const auto* entry =
                    table + record * kVideoOutProbeEntrySize;
                std::uint32_t enabled = 0;
                std::uint64_t pointer = 0;
                std::memcpy(
                    &enabled,
                    entry + 0x00,
                    sizeof(enabled));
                std::memcpy(
                    &pointer,
                    entry + 0x08,
                    sizeof(pointer));

                if (enabled == 0 || enabled > 4)
                    continue;

                add_candidate(
                    address + local,
                    record,
                    pointer);
            }
        }
    }

    sampler_log(
        "Sampler dynamic scan pid=%d module=0x%llx range=0x%llx-0x%llx "
        "candidates=%zu",
        pid_,
        static_cast<unsigned long long>(module_base_),
        static_cast<unsigned long long>(module_base_ + kScanBegin),
        static_cast<unsigned long long>(module_base_ + kScanEnd),
        candidate_count);

    if (candidate_count == 0)
        return false;

    platform_.sleep_ms(250);
    for (std::size_t i = 0; i < candidate_count; ++i) {
        candidates[i].second_ok = platform_.read_memory(
            pid_,
            candidates[i].counter,
            &candidates[i].second,
            sizeof(candidates[i].second));
    }

    platform_.sleep_ms(250);
    for (std::size_t i = 0; i < candidate_count; ++i) {
        if (!candidates[i].second_ok)
            continue;
        candidates[i].third_ok = platform_.read_memory(
            pid_,
            candidates[i].counter,
            &candidates[i].third,
            sizeof(candidates[i].third));
    }

    std::size_t selected = candidate_count;
    unsigned selected_score = std::numeric_limits<unsigned>::max();
    unsigned selected_delta1 = 0;
    unsigned selected_delta2 = 0;

    for (std::size_t i = 0; i < candidate_count; ++i) {
        const Candidate& candidate = candidates[i];
        if (!candidate.second_ok || !candidate.third_ok)
            continue;

        const std::uint32_t delta1 =
            static_cast<std::uint32_t>(
                candidate.second - candidate.first);
        const std::uint32_t delta2 =
            static_cast<std::uint32_t>(
                candidate.third - candidate.second);

        if (delta1 == 0 || delta1 > 40 ||
            delta2 == 0 || delta2 > 40) {
            continue;
        }

        const unsigned jitter =
            unsigned_distance(delta1, delta2);
        if (jitter > 4)
            continue;

        /* Prefer stable rates near common 30/60/120 Hz modes. */
        const unsigned estimated_fps =
            static_cast<unsigned>(delta1 + delta2) * 2U;
        const unsigned rate_distance =
            std::min(
                unsigned_distance(estimated_fps, 30U),
                std::min(
                    unsigned_distance(estimated_fps, 60U),
                    unsigned_distance(estimated_fps, 120U)));
        const unsigned score = jitter * 100U + rate_distance;

        if (score < selected_score) {
            selected = i;
            selected_score = score;
            selected_delta1 = delta1;
            selected_delta2 = delta2;
        }
    }

    if (selected == candidate_count) {
        sampler_log(
            "Sampler dynamic validation failed pid=%d candidates=%zu",
            pid_,
            candidate_count);
        return false;
    }

    const Candidate& winner = candidates[selected];
    counter_address_ = winner.counter;
    sampler_log(
        "Sampler dynamic chain selected pid=%d table=0x%llx "
        "offset=0x%llx record=%zu pointer=0x%llx root=0x%llx "
        "counter=0x%llx deltas=%u/%u estimated_fps=%u",
        pid_,
        static_cast<unsigned long long>(winner.table),
        static_cast<unsigned long long>(winner.table - module_base_),
        winner.record,
        static_cast<unsigned long long>(winner.pointer),
        static_cast<unsigned long long>(winner.root),
        static_cast<unsigned long long>(winner.counter),
        selected_delta1,
        selected_delta2,
        (selected_delta1 + selected_delta2) * 2U);
    return true;
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

    /*
     * The hardware-proven FW 9.60 producer discards one complete delta after
     * establishing its initial baseline. This prevents a startup/scheduling
     * spike from ever reaching the overlay.
     */
    if (warmup_deltas_remaining_ != 0) {
        --warmup_deltas_remaining_;
        return std::nullopt;
    }

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

#if !defined(PS5)
#undef sampler_log
#endif

} // namespace common_fps
