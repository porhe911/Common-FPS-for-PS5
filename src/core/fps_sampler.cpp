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
        "/data/CommonFPS_universal_stage8_1.log",
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
#if defined(PS5)
    const std::uint64_t attempt_time_us = platform_.monotonic_us();
    if (pid == discovery_failed_pid_ &&
        attempt_time_us < discovery_retry_after_us_) {
        return false;
    }
#endif

    reset();

    const auto module = platform_.find_module(pid, "libSceVideoOut.sprx");
    if (!module || module->base == 0)
        return false;

    pid_ = pid;
    module_base_ = module->base;

    if (!resolve_counter_address()) {
#if defined(PS5)
        discovery_failed_pid_ = pid;
        discovery_retry_after_us_ =
            platform_.monotonic_us() + 10'000'000ULL;
#endif
        reset();
        return false;
    }

#if defined(PS5)
    discovery_failed_pid_ = -1;
    discovery_retry_after_us_ = 0;
#endif
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
     * FW 7.60 proved that the 9.60 table offset may contain only zeroes.
     * Stage 8.1 therefore scans a wider read-only VideoOut image window and
     * accepts four conservative record layouts.  Every possible chain is
     * still rejected unless its uint32 counter advances at a display-like
     * rate in two independent 250 ms windows.
     *
     * No game memory is written by this discovery path.
     */
    constexpr std::uintptr_t kScanBegin = 0x10000;
    constexpr std::uintptr_t kScanEnd = 0x200000;
    constexpr std::size_t kScanStep = 0x2000;
    constexpr std::size_t kLayoutLookahead = 0x20;
    constexpr std::size_t kScanReadSize =
        kScanStep + kLayoutLookahead;
    constexpr std::size_t kMaxCandidates = 128;

    struct Layout {
        std::size_t enabled_offset;
        std::size_t pointer_offset;
        const char* name;
    };

    constexpr std::array<Layout, 4> kLayouts{{
        {0x00, 0x08, "e0-p8"},
        {0x04, 0x08, "e4-p8"},
        {0x00, 0x10, "e0-p10"},
        {0x04, 0x10, "e4-p10"},
    }};

    struct Candidate {
        std::uintptr_t record = 0;
        const char* layout = "none";
        std::uint64_t pointer = 0;
        std::uint64_t root = 0;
        std::uintptr_t counter = 0;
        unsigned chain_depth = 0;
        std::uint32_t first = 0;
        std::uint32_t second = 0;
        std::uint32_t third = 0;
        bool second_ok = false;
        bool third_ok = false;
    };

    std::array<Candidate, kMaxCandidates> candidates{};
    std::size_t candidate_count = 0;
    std::size_t pointer_matches = 0;
    std::size_t indirect_roots = 0;
    bool candidates_truncated = false;
    std::array<std::uint8_t, kScanReadSize> window{};

    auto add_candidate = [&](std::uintptr_t record_address,
                             const char* layout_name,
                             std::uint64_t pointer,
                             std::uint64_t root,
                             unsigned chain_depth) {
        if (!plausible_user_pointer(root))
            return;

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

        if (candidate_count == candidates.size()) {
            candidates_truncated = true;
            return;
        }

        Candidate& candidate = candidates[candidate_count++];
        candidate.record = record_address;
        candidate.layout = layout_name;
        candidate.pointer = pointer;
        candidate.root = root;
        candidate.counter = counter;
        candidate.chain_depth = chain_depth;
        candidate.first = first;
    };

    for (std::uintptr_t offset = kScanBegin;
         offset < kScanEnd;
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
             local + kLayoutLookahead <= window.size();
             local += sizeof(std::uint64_t)) {
            const auto* record = window.data() + local;

            for (const Layout& layout : kLayouts) {
                std::uint32_t enabled = 0;
                std::uint64_t pointer = 0;
                std::memcpy(
                    &enabled,
                    record + layout.enabled_offset,
                    sizeof(enabled));
                std::memcpy(
                    &pointer,
                    record + layout.pointer_offset,
                    sizeof(pointer));

                if (enabled == 0 || enabled > 8 ||
                    !plausible_user_pointer(pointer)) {
                    continue;
                }

                ++pointer_matches;

                /*
                 * Known 9.60 builds use one pointer indirection.  Also test a
                 * direct object pointer because older VideoOut layouts may
                 * store the object itself in the record.
                 */
                std::uint64_t indirect_root = 0;
                if (platform_.read_memory(
                        pid_,
                        static_cast<std::uintptr_t>(pointer),
                        &indirect_root,
                        sizeof(indirect_root)) &&
                    plausible_user_pointer(indirect_root)) {
                    ++indirect_roots;
                    add_candidate(
                        address + local,
                        layout.name,
                        pointer,
                        indirect_root,
                        2);
                }

                add_candidate(
                    address + local,
                    layout.name,
                    pointer,
                    pointer,
                    1);
            }
        }
    }

    sampler_log(
        "Sampler wide scan pid=%d module=0x%llx range=0x%llx-0x%llx "
        "pointer_matches=%zu indirect_roots=%zu candidates=%zu "
        "truncated=%d",
        pid_,
        static_cast<unsigned long long>(module_base_),
        static_cast<unsigned long long>(module_base_ + kScanBegin),
        static_cast<unsigned long long>(module_base_ + kScanEnd),
        pointer_matches,
        indirect_roots,
        candidate_count,
        candidates_truncated ? 1 : 0);

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
    [[maybe_unused]] unsigned selected_delta1 = 0;
    [[maybe_unused]] unsigned selected_delta2 = 0;

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

        if (i < 8) {
            sampler_log(
                "Sampler wide candidate pid=%d index=%zu layout=%s "
                "depth=%u record=0x%llx pointer=0x%llx root=0x%llx "
                "counter=0x%llx values=%u/%u/%u deltas=%u/%u",
                pid_,
                i,
                candidate.layout,
                candidate.chain_depth,
                static_cast<unsigned long long>(candidate.record),
                static_cast<unsigned long long>(candidate.pointer),
                static_cast<unsigned long long>(candidate.root),
                static_cast<unsigned long long>(candidate.counter),
                candidate.first,
                candidate.second,
                candidate.third,
                delta1,
                delta2);
        }

        if (delta1 == 0 || delta1 > 45 ||
            delta2 == 0 || delta2 > 45) {
            continue;
        }

        const unsigned jitter =
            unsigned_distance(delta1, delta2);
        if (jitter > 4)
            continue;

        const unsigned estimated_fps =
            static_cast<unsigned>(delta1 + delta2) * 2U;
        const unsigned rate_distance =
            std::min(
                unsigned_distance(estimated_fps, 30U),
                std::min(
                    unsigned_distance(estimated_fps, 60U),
                    unsigned_distance(estimated_fps, 120U)));

        /*
         * Prefer the proven indirect chain when two counters have the same
         * rate and jitter.  The direct form remains a compatibility fallback.
         */
        const unsigned chain_penalty =
            candidate.chain_depth == 2 ? 0U : 5U;
        const unsigned score =
            jitter * 1000U + rate_distance * 10U + chain_penalty;

        if (score < selected_score) {
            selected = i;
            selected_score = score;
            selected_delta1 = delta1;
            selected_delta2 = delta2;
        }
    }

    if (selected == candidate_count) {
        sampler_log(
            "Sampler wide validation failed pid=%d candidates=%zu",
            pid_,
            candidate_count);
        return false;
    }

    const Candidate& winner = candidates[selected];
    counter_address_ = winner.counter;
    sampler_log(
        "Sampler wide chain selected pid=%d layout=%s depth=%u "
        "record=0x%llx offset=0x%llx pointer=0x%llx root=0x%llx "
        "counter=0x%llx deltas=%u/%u estimated_fps=%u",
        pid_,
        winner.layout,
        winner.chain_depth,
        static_cast<unsigned long long>(winner.record),
        static_cast<unsigned long long>(winner.record - module_base_),
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
