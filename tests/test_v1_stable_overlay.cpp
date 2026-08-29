#include "common_fps/v1_stable_overlay.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

int main() {
    using namespace common_fps::v1_stable;

    assert(std::string(kLabelText) == "FPS:");
    assert(std::string(kLoadingVisibleText) == "FPS: loading");
    assert(std::string(kNumericFormat) == "%.0f");
    assert(kFontSize == 26);
    assert(kRawCursorX == 10.0f);
    assert(kRawCursorY == 1038.0f);

    assert(kLabelColor.r == 0xB3);
    assert(kLabelColor.g == 0x66);
    assert(kLabelColor.b == 0xFF);
    assert(kLabelColor.a == 0xFF);

    assert(kValueColor.r == 0xFF);
    assert(kValueColor.g == 0xFF);
    assert(kValueColor.b == 0xFF);
    assert(kValueColor.a == 0xFF);

    assert(std::fabs(initial_cursor_y(1920.0f) - 1070.0f) < 0.001f);

    char value[32]{};
    std::snprintf(value, sizeof(value), kNumericFormat, 59.6);
    assert(std::string(value) == "60");

    return 0;
}
