/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace common_fps {

enum class SceneState {
    WaitingForScene,
    StabilizingScene,
    CreatingWidgets,
    Ready,
};

struct ScenePolicy {
    unsigned consecutive_ready_samples = 2;
};

class SceneBackend {
public:
    virtual ~SceneBackend() = default;

    /*
     * True only when Scene.RootWidget (or the equivalent source-built PUI
     * root) is available.
     */
    virtual bool scene_ready() = 0;

    /*
     * True when Common FPS widgets are currently alive in ShellUI.
     */
    virtual bool widgets_alive() = 0;

    /*
     * Create the two Common FPS widgets. Must be idempotent or first remove
     * stale Common FPS widgets by their IDs.
     */
    virtual bool create_widgets_once() = 0;
};

class SceneGuard {
public:
    SceneGuard(
        SceneBackend& backend,
        ScenePolicy policy = {});

    SceneState tick();

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] SceneState state() const noexcept;

    void reset();

private:
    SceneBackend& backend_;
    ScenePolicy policy_;
    SceneState state_ = SceneState::WaitingForScene;
    unsigned consecutive_ready_ = 0;
};

} // namespace common_fps
