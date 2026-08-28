/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/autoload_guard.hpp"

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

class Ps5Platform;

/*
 * Real PS5 backend for the v1.1.0 autoload state machine.
 *
 * Safety contract:
 *   - target only SceShellUI;
 *   - never attach/read/write the game process here;
 *   - wait until ShellUI + Mono runtime are present;
 *   - inject with one continuous ptrace session;
 *   - use elfldr_debug(), not elfldr_exec(), so an injector failure never
 *     SIGKILLs SceShellUI;
 *   - confirm the injected renderer by loopback heartbeat;
 *   - never inject a second renderer while the process-lifetime sentinel from
 *     an earlier copy is still owned by the same live SceShellUI process.
 */
class Ps5AutoloadBackend final : public common_fps::AutoloadBackend {
public:
    explicit Ps5AutoloadBackend(Ps5Platform& platform);
    ~Ps5AutoloadBackend() override;

    Ps5AutoloadBackend(const Ps5AutoloadBackend&) = delete;
    Ps5AutoloadBackend& operator=(const Ps5AutoloadBackend&) = delete;

    [[nodiscard]] bool valid() const noexcept;

    bool runtime_ready() override;
    bool renderer_alive() override;
    bool install_renderer_once() override;

    /*
     * Stronger gate than renderer_alive().
     *
     * renderer_alive() means the injected companion is running and its
     * receiver/heartbeat loop is healthy. visual_ready() means the ShellUI
     * side has also completed its safe Mono/PUI main-thread bootstrap.
     *
     * The game lifecycle must not start before this becomes true.
     */
    [[nodiscard]] bool visual_ready();

private:
    bool refresh_shellui();
    bool mono_runtime_present(pid_t pid);
    bool open_health_socket();
    bool renderer_sentinel_present() const;
    void reset_heartbeat_state(pid_t new_pid);
    void drain_health();
    bool heartbeat_fresh(std::uint64_t timestamp_us) const;
    bool visual_heartbeat_fresh(std::uint64_t timestamp_us) const;
    bool read_renderer_elf(unsigned char** data, std::size_t* size) const;

    Ps5Platform& platform_;
    int health_socket_ = -1;
    pid_t shellui_pid_ = -1;

    std::uint64_t last_receiver_heartbeat_us_ = 0;
    std::uint64_t last_visual_heartbeat_us_ = 0;
    std::uint64_t last_health_sequence_ = 0;

    pid_t last_injected_pid_ = -1;
    std::uint64_t last_injection_attempt_us_ = 0;
};

} // namespace common_fps::ps5
