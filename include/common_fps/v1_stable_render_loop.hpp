/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include "common_fps/v1_stable_hook_state.hpp"

#include <cstdint>

namespace common_fps::v1_stable {

class RenderActions {
public:
    virtual ~RenderActions() = default;

    virtual bool create_fps_label() = 0;
    virtual void set_fps_text_raw(const char* text) = 0;
    virtual void set_fps_text(const char* text, const char* text2) = 0;
};

/*
 * UI-independent reconstruction of the stable v1.0.0 OnRender_Hook body.
 *
 * The actual ShellUI adapter will call this only from the existing PUI update
 * callback, then invoke the original callback. Keeping the state machine here
 * makes the behavior host-testable without loading or mutating PS5 UI state.
 */
class RenderLoop {
public:
    void update(const HookState& state, RenderActions& actions);

    std::uint32_t tick() const noexcept { return tick_; }
    bool label_ready() const noexcept { return label_ready_; }
    std::uint64_t last_sequence() const noexcept { return last_sequence_; }
    std::uint64_t last_raw_sequence() const noexcept {
        return last_raw_sequence_;
    }

private:
    static constexpr std::uint32_t kLabelDelayTicks = 30;

    std::uint32_t tick_ = 0;
    bool label_ready_ = false;
    std::uint64_t last_sequence_ = 0;
    std::uint64_t last_raw_sequence_ = 0;
};

} // namespace common_fps::v1_stable
