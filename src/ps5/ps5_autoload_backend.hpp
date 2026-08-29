/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "common_fps/autoload_guard.hpp"

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5 {

class Ps5Platform;

class Ps5AutoloadBackend final : public common_fps::AutoloadBackend {
public:
    explicit Ps5AutoloadBackend(Ps5Platform& platform);
    ~Ps5AutoloadBackend() override;

    [[nodiscard]] bool valid() const noexcept;

    bool runtime_ready() override;
    bool renderer_alive() override;
    bool install_renderer_once() override;
    [[nodiscard]] bool visual_ready();

private:
    bool refresh_shellui();
    bool mono_runtime_present(pid_t pid);
    bool open_health_socket();
    void reset_heartbeat_state(pid_t new_pid);
    void drain_health();
    bool heartbeat_fresh(std::uint64_t timestamp_us) const;
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
