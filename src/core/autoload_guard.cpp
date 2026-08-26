/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/autoload_guard.hpp"

#include <algorithm>

namespace common_fps {

AutoloadGuard::AutoloadGuard(
    AutoloadBackend& backend,
    AutoloadPolicy policy)
    : backend_(backend),
      policy_(policy) {

    if (policy_.consecutive_ready_samples == 0)
        policy_.consecutive_ready_samples = 1;
}

bool AutoloadGuard::ready() const noexcept {
    return state_ == AutoloadState::Ready;
}

AutoloadState AutoloadGuard::state() const noexcept {
    return state_;
}

unsigned AutoloadGuard::consecutive_ready_samples() const noexcept {
    return consecutive_ready_;
}

unsigned AutoloadGuard::install_attempts() const noexcept {
    return install_attempts_;
}

void AutoloadGuard::reset() {
    state_ = AutoloadState::WaitingForRuntime;
    consecutive_ready_ = 0;
    install_attempts_ = 0;
    retry_not_before_us_ = 0;
}

void AutoloadGuard::schedule_retry(std::uint64_t now_us) {
    state_ = AutoloadState::WaitingToRetry;
    consecutive_ready_ = 0;
    retry_not_before_us_ = now_us + policy_.retry_delay_us;
}

AutoloadState AutoloadGuard::tick(std::uint64_t now_us) {
    /*
     * Self-healing behavior:
     *
     * If the renderer was once healthy but later disappears, do not leave the
     * plugin in a false "enabled but no FPS" state. Return to the readiness
     * gate and reinstall it.
     */
    if (state_ == AutoloadState::Ready) {
        if (backend_.renderer_alive())
            return state_;

        schedule_retry(now_us);
        return state_;
    }

    /*
     * While waiting after a failed install, do not hammer ShellUI.
     */
    if (state_ == AutoloadState::WaitingToRetry &&
        now_us < retry_not_before_us_) {
        return state_;
    }

    /*
     * Never install against an early/unstable ShellUI runtime.
     */
    if (!backend_.runtime_ready()) {
        state_ = AutoloadState::WaitingForRuntime;
        consecutive_ready_ = 0;
        return state_;
    }

    state_ = AutoloadState::StabilizingRuntime;
    ++consecutive_ready_;

    if (consecutive_ready_ < policy_.consecutive_ready_samples)
        return state_;

    /*
     * An already-running renderer is valid. This also prevents duplicates
     * after a plugin stop/start race.
     */
    if (backend_.renderer_alive()) {
        state_ = AutoloadState::Ready;
        return state_;
    }

    state_ = AutoloadState::InstallingRenderer;
    ++install_attempts_;

    if (!backend_.install_renderer_once()) {
        schedule_retry(now_us);
        return state_;
    }

    /*
     * Do not trust only the return code from the injector. Require a health
     * confirmation so "installed" cannot mean "worker already died".
     */
    if (!backend_.renderer_alive()) {
        schedule_retry(now_us);
        return state_;
    }

    state_ = AutoloadState::Ready;
    consecutive_ready_ = policy_.consecutive_ready_samples;
    return state_;
}

} // namespace common_fps
