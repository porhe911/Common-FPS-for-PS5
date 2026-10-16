/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "common_fps/constants.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace common_fps {

enum class FpsDisplaySource { None, VideoOut, DceFixed, DceAdaptive, SharedHen };

// Presentation only: never alters counters or assumes a game's FPS cap.
class FpsDisplayFilter {
public:
    std::optional<int> sample(std::optional<int> measured, std::uint64_t now_us,
                             int pid, FpsDisplaySource source) noexcept {
        if (pid <= 0 || source == FpsDisplaySource::None || !measured ||
            *measured < 0 || *measured > kMaxDisplayFps) {
            reset();
            return std::nullopt;
        }
        if (!have_epoch_ || pid != pid_ || source != source_ ||
            now_us < previous_us_ || now_us - previous_us_ > kMaxFpsWindowUs) {
            reset();
            have_epoch_ = true;
            pid_ = pid;
            source_ = source;
            started_us_ = previous_us_ = now_us;
        } else if (now_us == previous_us_) {
            return displayed_; // one timestamp cannot confirm multiple samples
        }
        previous_us_ = now_us;

        if (!displayed_) {
            recent_[recent_cursor_] = *measured;
            recent_cursor_ = (recent_cursor_ + 1) % kWindow;
            recent_count_ = std::min(kWindow, recent_count_ + 1);
            if (now_us - started_us_ < kStartupUs || recent_count_ < kWindow)
                return std::nullopt;
            displayed_ = median(recent_);
            accepted_.fill(*displayed_);
            return displayed_;
        }

        // A one-sample drop remains visible. Retaining the last three accepted
        // values lets recovery from that drop appear immediately as well.
        const int reference = *std::max_element(accepted_.begin(), accepted_.end());
        const int upward_threshold = reference + std::max(8, reference / 5);
        if (*measured > upward_threshold) {
            if (elevated_count_ == 0)
                elevated_since_us_ = now_us;
            elevated_[elevated_cursor_] = *measured;
            elevated_cursor_ = (elevated_cursor_ + 1) % kWindow;
            elevated_count_ = std::min(kWindow, elevated_count_ + 1);
            if (elevated_count_ < kWindow || now_us - elevated_since_us_ < kRiseUs)
                return displayed_;
            // Three consecutive higher observations confirm a real increase,
            // including variable rates and genuine 120 FPS, without a 60 cap.
            displayed_ = median(elevated_);
            accepted_.fill(*displayed_);
            accepted_cursor_ = 0;
        } else {
            displayed_ = *measured;
            accepted_[accepted_cursor_] = *measured;
            accepted_cursor_ = (accepted_cursor_ + 1) % kWindow;
        }
        elevated_count_ = elevated_cursor_ = 0;
        return displayed_;
    }

    void reset() noexcept { *this = FpsDisplayFilter{}; }

private:
    static constexpr unsigned kWindow = 3;
    static constexpr std::uint64_t kStartupUs = 5'000'000ULL;
    static constexpr std::uint64_t kRiseUs = 2'000'000ULL;
    static int median(std::array<int, kWindow> values) noexcept {
        std::sort(values.begin(), values.end());
        return values[kWindow / 2];
    }
    bool have_epoch_ = false;
    int pid_ = -1;
    FpsDisplaySource source_ = FpsDisplaySource::None;
    std::uint64_t started_us_ = 0, previous_us_ = 0, elevated_since_us_ = 0;
    std::optional<int> displayed_;
    std::array<int, kWindow> recent_{}, accepted_{}, elevated_{};
    unsigned recent_count_ = 0, recent_cursor_ = 0, accepted_cursor_ = 0;
    unsigned elevated_count_ = 0, elevated_cursor_ = 0;
};
} // namespace common_fps
