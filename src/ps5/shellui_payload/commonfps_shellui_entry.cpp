/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "common_fps/renderer_health.hpp"
#include "HookedFuncs.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

MonoImage* pui_img = nullptr;
MonoObject* Game = nullptr;

namespace {

int open_health_sender()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void send_health(int socket_fd, std::uint64_t sequence)
{
    if (socket_fd < 0)
        return;

    common_fps::RendererHealthPacket packet{};
    packet.shellui_pid = static_cast<std::int32_t>(getpid());
    packet.phase = static_cast<std::uint16_t>(
        common_fps::RendererHealthPhase::ReceiverReady);
    packet.sequence = sequence;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(common_fps::kRendererHealthPort);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sendto(
        socket_fd,
        &packet,
        sizeof(packet),
        0,
        reinterpret_cast<sockaddr*>(&target),
        sizeof(target));
}

} // namespace

int main(int, const char**)
{
    using namespace common_fps::ps5::shellui;

    // Stage A intentionally starts only the local receiver and heartbeat.
    // PUI work remains disabled until the later main-thread bootstrap stage.
    if (!initialize_receiver())
        return 1;

    const int health_socket = open_health_sender();
    std::uint64_t health_sequence = 1;
    unsigned ticks_until_health = 0;

    for (;;) {
        if (ticks_until_health == 0) {
            send_health(health_socket, health_sequence++);
            ticks_until_health = 16;
        }

        --ticks_until_health;
        usleep(16000);
    }

    return 0;
}
