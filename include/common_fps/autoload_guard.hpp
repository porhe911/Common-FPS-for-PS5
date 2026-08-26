/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace common_fps {

enum class AutoloadState {
    WaitingForRuntime,
    StabilizingRuntime,
    WaitingToRetry,
    InstallingRenderer,
    Ready,
};

struct AutoloadPolicy {
    /*
     * Do not inject the renderer after a single positive readiness check.
     * Three consecutive checks at 250 ms polling means roughly 0.5 s of
     * continuously-ready runtime before the first install attempt.
     */
    unsigned consecutive_ready_samples = 3;

    /*
     * If installation fails, keep the worker alive and retry later.
     * This prevents the etaHEN UI from showing an "enabled" plugin whose
     * renderer worker has already died.
     */
    std::uint64_t retry_delay_us = 1'500'000ULL;
};

class AutoloadBackend {
public:
    virtual ~AutoloadBackend() = default;

    /*
     * True only when the ShellUI process and the prerequisites required by
     * the source-built loader are available.
     *
     * This must not touch the game process.
     */
    virtual bool runtime_ready() = 0;

    /*
     * Health check for the renderer instance.
     */
    virtual bool renderer_alive() = 0;

    /*
     * Perform one complete v1.0.0-compatible renderer installation attempt.
     * The implementation must preserve:
     *   - Scene/runtime readiness contract
     *   - payload args
     *   - one continuous loader session
     */
    virtual bool install_renderer_once() = 0;
};

class AutoloadGuard {
public:
    AutoloadGuard(
        AutoloadBackend& backend,
        AutoloadPolicy policy = {});

    AutoloadState tick(std::uint64_t now_us);

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] AutoloadState state() const noexcept;
    [[nodiscard]] unsigned consecutive_ready_samples() const noexcept;
    [[nodiscard]] unsigned install_attempts() const noexcept;

    void reset();

private:
    void schedule_retry(std::uint64_t now_us);

    AutoloadBackend& backend_;
    AutoloadPolicy policy_;

    AutoloadState state_ = AutoloadState::WaitingForRuntime;
    unsigned consecutive_ready_ = 0;
    unsigned install_attempts_ = 0;
    std::uint64_t retry_not_before_us_ = 0;
};

} // namespace common_fps
