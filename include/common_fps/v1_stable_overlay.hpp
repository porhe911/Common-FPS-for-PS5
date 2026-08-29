/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include <cstdint>

namespace common_fps::v1_stable {

inline constexpr const char* kLabelText = "FPS:";
inline constexpr const char* kNumericFormat = "%.0f";
inline constexpr const char* kLoadingVisibleText = "FPS: loading";

inline constexpr int kFontSize = 26;

// Stable v1.0.0 raw-text parser resets these before laying out each update.
inline constexpr float kRawCursorX = 10.0f;
inline constexpr float kRawCursorY = 1038.0f;

struct Rgba8 {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

inline constexpr Rgba8 kLabelColor{0xB3, 0x66, 0xFF, 0xFF};
inline constexpr Rgba8 kValueColor{0xFF, 0xFF, 0xFF, 0xFF};

// Recovered from the stable phu_create_fps_label path. With a 1920-wide
// logical root this evaluates to 1070.0 before the label-specific layout work.
inline constexpr float kInitialCursorX = 4.0f;
inline constexpr float kInitialCursorScale = 0.5625f;
inline constexpr float kInitialCursorMargin = 10.0f;

constexpr float initial_cursor_y(float root_width) noexcept {
    return root_width * kInitialCursorScale - kInitialCursorMargin;
}

} // namespace common_fps::v1_stable
