/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "common_fps/constants.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace common_fps {

/* Pure DCE counter logic shared by the PS5 adapter and host regressions. */
class DceCounterSampler {
public:
    std::optional<int> sample(
        const std::array<std::uint64_t, 12>& words,
        std::uint64_t now_us) noexcept;
    void reset() noexcept;
    int selected_word() const noexcept { return selected_word_; }

private:
    bool have_baseline_ = false;
    unsigned warmup_deltas_remaining_ = kWarmupDeltasToDiscard;
    std::array<std::uint64_t, 12> previous_words_{};
    std::uint64_t previous_time_us_ = 0;
    std::array<unsigned, 12> candidate_streak_{};
    std::array<int, 12> candidate_last_fps_{};
    int selected_word_ = -1;
    unsigned selected_invalid_ = 0;
};

} // namespace common_fps
