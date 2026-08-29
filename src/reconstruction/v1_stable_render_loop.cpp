/* Common FPS for PS5 - GPL-3.0-or-later */
#include "common_fps/v1_stable_render_loop.hpp"

namespace common_fps::v1_stable {

void RenderLoop::update(const HookState& state, RenderActions& actions) {
    if (!label_ready_ && tick_ >= kLabelDelayTicks) {
        label_ready_ = actions.create_fps_label();
    }

    if (label_ready_) {
        RawSnapshot raw{};
        if (state.snapshot_raw(raw) &&
            raw.sequence != 0 &&
            raw.sequence != last_raw_sequence_) {
            actions.set_fps_text_raw(raw.text.data());
            last_raw_sequence_ = raw.sequence;
        }

        TextSnapshot text{};
        if (state.snapshot(text) &&
            text.sequence != 0 &&
            text.sequence != last_sequence_) {
            actions.set_fps_text(text.text.data(), text.text2.data());
            last_sequence_ = text.sequence;
        }
    }

    ++tick_;
}

} // namespace common_fps::v1_stable
