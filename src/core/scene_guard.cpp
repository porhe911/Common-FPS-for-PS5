/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/scene_guard.hpp"

namespace common_fps {

SceneGuard::SceneGuard(
    SceneBackend& backend,
    ScenePolicy policy)
    : backend_(backend),
      policy_(policy) {

    if (policy_.consecutive_ready_samples == 0)
        policy_.consecutive_ready_samples = 1;
}

bool SceneGuard::ready() const noexcept {
    return state_ == SceneState::Ready;
}

SceneState SceneGuard::state() const noexcept {
    return state_;
}

void SceneGuard::reset() {
    state_ = SceneState::WaitingForScene;
    consecutive_ready_ = 0;
}

SceneState SceneGuard::tick() {
    if (state_ == SceneState::Ready) {
        if (backend_.widgets_alive())
            return state_;

        /*
         * ShellUI may have recreated its Scene. Do not die; wait for the new
         * RootWidget and recreate only Common FPS widgets.
         */
        reset();
    }

    if (!backend_.scene_ready()) {
        state_ = SceneState::WaitingForScene;
        consecutive_ready_ = 0;
        return state_;
    }

    state_ = SceneState::StabilizingScene;
    ++consecutive_ready_;

    if (consecutive_ready_ < policy_.consecutive_ready_samples)
        return state_;

    if (backend_.widgets_alive()) {
        state_ = SceneState::Ready;
        return state_;
    }

    state_ = SceneState::CreatingWidgets;

    if (!backend_.create_widgets_once()) {
        /*
         * Important autoload property:
         * a one-time early failure is not terminal.
         */
        state_ = SceneState::WaitingForScene;
        consecutive_ready_ = 0;
        return state_;
    }

    if (!backend_.widgets_alive()) {
        state_ = SceneState::WaitingForScene;
        consecutive_ready_ = 0;
        return state_;
    }

    state_ = SceneState::Ready;
    return state_;
}

} // namespace common_fps
