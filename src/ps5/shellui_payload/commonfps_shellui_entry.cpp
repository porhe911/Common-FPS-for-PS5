/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "common_fps/renderer_health.hpp"

#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Diagnostic RC2: injection/heartbeat only.
 *
 * Do NOT touch Mono, Application.Update, PUI widgets or SceShellUI authid.
 * This isolates whether the embedded elfldr_debug injection itself is safe on
 * FW 9.60. If a game launches normally with this payload, the regression is
 * above the injection layer (Mono/detour/widget bootstrap), not in the FPS
 * sampler or controller lifecycle.
 */

MonoImage* pui_img = nullptr;
MonoImage* AppSystem_img = nullptr;
MonoObject* Game = nullptr;
bool has_hv_bypass = false;
bool is_testkit = false;

namespace {

void stage_log(const char* text) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_shellui.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

int open_health_sender() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void send_health(int fd, std::uint64_t seq) {
    if (fd < 0)
        return;

    common_fps::RendererHealthPacket packet{};
    packet.shellui_pid = static_cast<std::int32_t>(getpid());
    packet.phase = static_cast<std::uint16_t>(
        common_fps::RendererHealthPhase::ReceiverReady);
    packet.sequence = seq;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(common_fps::kRendererHealthPort);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    (void)sendto(fd, &packet, sizeof(packet), 0,
        reinterpret_cast<sockaddr*>(&target), sizeof(target));
}

} // namespace

int main(int, const char**) {
    stage_log("R0 RC2 injection smoke entered");

    if (!common_fps::ps5::shellui::initialize_receiver()) {
        stage_log("R0 FAIL receiver");
        return 1;
    }

    const int health_fd = open_health_sender();
    if (health_fd < 0) {
        stage_log("R1 FAIL health sender");
        return 2;
    }

    stage_log("R1 RC2 ReceiverReady; no Mono/no hook/no widgets");

    std::uint64_t sequence = 1;
    for (;;) {
        send_health(health_fd, sequence++);
        sleep(1);
    }
}
