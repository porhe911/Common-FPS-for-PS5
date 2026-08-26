/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/lifecycle.hpp"
#include "common_fps/wire.hpp"

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

    auto config = load_config();
    common_fps::Lifecycle lifecycle(platform, config);

    std::uint64_t sequence = 1;

    for (;;) {
        const auto frame = lifecycle.tick();
        sender.send(common_fps::make_wire_packet(frame, sequence++));
        platform.sleep_ms(1000);
    }
}

} // namespace

extern "C" int main() {
    /*
     * Preserve the v1.0.0 user experience:
     * parent returns immediately, worker continues initialization/lifecycle.
     */
    const pid_t pid = fork();

    if (pid > 0)
        return 0;

    return run_worker();
}
