#include "common_fps/v1_stable_hook_state.hpp"

#include <cassert>
#include <cstring>
#include <string>

int main() {
    using namespace common_fps::v1_stable;

    HookState state;
    assert(state.sequence() == 0);
    assert(state.raw_sequence() == 0);

    state.publish("FPS:", "60");
    assert(state.sequence() == 2);

    TextSnapshot text{};
    assert(state.snapshot(text));
    assert(text.sequence == 2);
    assert(std::strcmp(text.text.data(), "FPS:") == 0);
    assert(std::strcmp(text.text2.data(), "60") == 0);

    state.publish_raw("FPS: loading\n");
    assert(state.raw_sequence() == 2);

    RawSnapshot raw{};
    assert(state.snapshot_raw(raw));
    assert(raw.sequence == 2);
    assert(std::strcmp(raw.text.data(), "FPS: loading\n") == 0);

    // Stable buffers reserve one byte for the trailing NUL.
    std::string long_text(kHookTextCapacity + 100, 'A');
    state.publish_raw(long_text.c_str());
    assert(state.snapshot_raw(raw));
    assert(std::strlen(raw.text.data()) == kHookTextCapacity - 1);
    assert(raw.text[kHookTextCapacity - 1] == '\0');

    // Null publishes are ignored, matching the donor guards.
    const auto seq_before = state.sequence();
    state.publish(nullptr, "x");
    assert(state.sequence() == seq_before);

    return 0;
}
