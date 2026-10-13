/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/dce_counter_sampler.hpp"
#include "common_fps/fps_rate.hpp"

namespace common_fps {

std::optional<int> DceCounterSampler::sample(
    const std::array<std::uint64_t, 12>& words,
    std::uint64_t now_us) noexcept {

    if (!have_baseline_) {
        previous_words_ = words;
        previous_time_us_ = now_us;
        have_baseline_ = true;
        return std::nullopt;
    }

    if (now_us <= previous_time_us_ ||
        now_us - previous_time_us_ > kMaxFpsWindowUs) {
        reset();
        previous_words_ = words;
        previous_time_us_ = now_us;
        have_baseline_ = true;
        return std::nullopt;
    }

    const std::uint64_t elapsed_us = now_us - previous_time_us_;
    if (elapsed_us < kMinFpsWindowUs)
        return std::nullopt;

    if (warmup_deltas_remaining_ != 0) {
        --warmup_deltas_remaining_;
        previous_words_ = words;
        previous_time_us_ = now_us;
        return std::nullopt;
    }

    auto fps_for_word = [&](std::size_t index) -> int {
        if (words[index] < previous_words_[index])
            return 0;
        const std::uint64_t delta =
            words[index] - previous_words_[index];
        const double fps =
            static_cast<double>(delta) * 1000000.0 /
            static_cast<double>(elapsed_us);
        return validated_fps(fps).value_or(0);
    };

    /*
     * The known layout places the flip counter at +0x08. Keep that fast path
     * first. If firmware changed the returned structure, discover a stable
     * monotonic counter across the remaining aligned 64-bit fields.
     */
    int chosen_fps = 0;

    const int fixed_fps = fps_for_word(1);
    if (fixed_fps != 0) {
        chosen_fps = fixed_fps;
        selected_word_ = 1;
        selected_invalid_ = 0;
    } else if (selected_word_ >= 0 &&
               selected_word_ < static_cast<int>(words.size())) {
        chosen_fps = fps_for_word(
            static_cast<std::size_t>(selected_word_));
        if (chosen_fps == 0) {
            ++selected_invalid_;
            if (selected_invalid_ >= 3) {
                selected_word_ = -1;
                selected_invalid_ = 0;
            }
        } else {
            selected_invalid_ = 0;
        }
    }

    if (chosen_fps == 0 && selected_word_ < 0) {
        unsigned best_streak = 0;
        int best_word = -1;
        int best_fps = 0;

        for (std::size_t i = 0; i < words.size(); ++i) {
            const int fps = fps_for_word(i);
            if (fps == 0) {
                candidate_streak_[i] = 0;
                candidate_last_fps_[i] = 0;
                continue;
            }

            const int previous = candidate_last_fps_[i];
            const int difference =
                previous > fps ? previous - fps : fps - previous;

            if (previous != 0 && difference <= 12)
                ++candidate_streak_[i];
            else
                candidate_streak_[i] = 1;

            candidate_last_fps_[i] = fps;

            if (candidate_streak_[i] >= 2 &&
                candidate_streak_[i] > best_streak) {
                best_streak = candidate_streak_[i];
                best_word = static_cast<int>(i);
                best_fps = fps;
            }
        }

        if (best_word >= 0) {
            selected_word_ = best_word;
            chosen_fps = best_fps;
        }
    }

    previous_words_ = words;
    previous_time_us_ = now_us;

    if (chosen_fps == 0)
        return std::nullopt;

    return chosen_fps;
}

void DceCounterSampler::reset() noexcept {
    have_baseline_ = false;
    previous_words_.fill(0);
    previous_time_us_ = 0;
    warmup_deltas_remaining_ = kWarmupDeltasToDiscard;
    candidate_streak_.fill(0);
    candidate_last_fps_.fill(0);
    selected_word_ = -1;
    selected_invalid_ = 0;
}

} // namespace common_fps
