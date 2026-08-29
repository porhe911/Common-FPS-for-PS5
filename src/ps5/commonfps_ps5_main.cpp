/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/autoload_guard.hpp"
#include "common_fps/config.hpp"
#include "common_fps/lifecycle.hpp"
#include "common_fps/wire.hpp"

#include "ps5_autoload_backend.hpp"
#include "ps5_platform.hpp"
#include "state_sender.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace {

common_fps::OverlayConfig load_config() {
    constexpr const char* kPath = "/data/CommonFPS/config.ini";
    std::ifstream file(kPath);
    if (!file)
        return common_fps::default_config();
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return common_fps::parse_config_text(buffer.str());
}

void stage_log(const char* text) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_controller.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

int run_worker() {
    common_fps::ps5::Ps5Platform platform;
    common_fps::ps5::StateSender sender;
    common_fps::ps5::Ps5AutoloadBackend autoload_backend(platform);

    if (!autoload_backend.valid()) {
        stage_log("C0 FAIL health socket / duplicate worker");
        return 0;
    }

    stage_log("C0 worker started StageB v11");

    auto config = load_config();
    common_fps::Lifecycle lifecycle(platform, config);

    common_fps::AutoloadPolicy policy;
    policy.consecutive_ready_samples = 8;
    policy.retry_delay_us = 5'000'000ULL;
    common_fps::AutoloadGuard guard(autoload_backend, policy);

    std::uint64_t sequence = 1;
    bool previously_operational = false;
    bool logged_receiver = false;
    bool logged_visual = false;

    for (;;) {
        const bool renderer_running =
            guard.tick(platform.monotonic_us()) == common_fps::AutoloadState::Ready;

        if (renderer_running && !logged_receiver) {
            stage_log("C1 ReceiverReady");
            logged_receiver = true;
        }

        const bool visual_ready = renderer_running && autoload_backend.visual_ready();
        if (visual_ready && !logged_visual) {
            stage_log("C2 VisualReady; FPS lifecycle enabled");
            logged_visual = true;
        }

        if (!visual_ready) {
            if (previously_operational)
                lifecycle.reset();
            previously_operational = false;
            platform.sleep_ms(250);
            continue;
        }

        previously_operational = true;
        const auto frame = lifecycle.tick();
        sender.send(common_fps::make_wire_packet(frame, sequence++));
        platform.sleep_ms(1000);
    }
}

} // namespace

extern "C" int main() {
    const pid_t pid = fork();
    if (pid > 0)
        return 0;
    if (pid < 0)
        return 1;
    return run_worker();
}
