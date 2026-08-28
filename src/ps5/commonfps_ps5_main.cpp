/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
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

int run_worker() {
    common_fps::ps5::Ps5Platform platform;
    common_fps::ps5::StateSender sender;
    common_fps::ps5::Ps5AutoloadBackend autoload_backend(platform);

    /*
     * If the local health port is already owned, another Common FPS worker is
     * probably still alive. Fail closed instead of spawning a duplicate
     * controller which could race the same ShellUI instance.
     */
    if (!autoload_backend.valid())
        return 0;

    auto config = load_config();
    common_fps::Lifecycle lifecycle(platform, config);

    common_fps::AutoloadPolicy autoload_policy;

    /*
     * FW 9.60 autoload is deliberately more conservative than host-test
     * defaults: eight 250 ms polls means ShellUI + Mono must stay present for
     * roughly 1.75 s before the first injection attempt.
     */
    autoload_policy.consecutive_ready_samples = 8;
    autoload_policy.retry_delay_us = 5'000'000ULL;

    common_fps::AutoloadGuard autoload_guard(
        autoload_backend,
        autoload_policy);

    std::uint64_t sequence = 1;
    bool previously_operational = false;

    for (;;) {
        const bool renderer_running =
            autoload_guard.tick(platform.monotonic_us()) ==
            common_fps::AutoloadState::Ready;

        /*
         * A receiver heartbeat only proves that the injected companion
         * survived. It does NOT yet prove that PUI is safe to touch.
         *
         * This second gate is release-critical: the normal game lifecycle is
         * not entered until the renderer reports VisualReady from the
         * ShellUI main/update thread bootstrap.
         */
        const bool visual_ready =
            renderer_running && autoload_backend.visual_ready();

        if (!visual_ready) {
            if (previously_operational)
                lifecycle.reset();

            previously_operational = false;
            platform.sleep_ms(250);
            continue;
        }

        previously_operational = true;

        /*
         * Only after the renderer/runtime side is healthy enter the ordinary
         * read-only v1.0.0 lifecycle:
         *
         *   wait eboot.bin -> sample -> reset on exit -> next eboot.bin
         */
        const auto frame = lifecycle.tick();
        sender.send(common_fps::make_wire_packet(frame, sequence++));

        platform.sleep_ms(1000);
    }
}

} // namespace

extern "C" int main() {
    /*
     * Preserve the accepted v0.28b/v1.0.0 async behavior:
     * etaHEN gets control back immediately while the child performs the
     * conservative readiness/injection sequence in the background.
     */
    const pid_t pid = fork();

    if (pid > 0)
        return 0;

    if (pid < 0)
        return 1;

    return run_worker();
}
