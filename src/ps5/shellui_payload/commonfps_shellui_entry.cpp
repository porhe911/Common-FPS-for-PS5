/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "commonfps_shellui_bootstrap.hpp"
#include "common_fps/renderer_health.hpp"

#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int open_health_sender()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

int open_injection_sentinel()
{
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        return -1;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(common_fps::kRendererSentinelPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(
            socket_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

void send_health(
    int socket_fd,
    std::uint64_t sequence,
    common_fps::RendererHealthPhase phase)
{
    if (socket_fd < 0)
        return;

    common_fps::RendererHealthPacket packet{};
    packet.shellui_pid = static_cast<std::int32_t>(getpid());
    packet.phase = static_cast<std::uint16_t>(phase);
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

    /*
     * This port is a process-lifetime ownership marker.  A second injected
     * copy in the same SceShellUI must exit before it can touch Mono/PUI.
     * This specifically makes etaHEN Stop -> Run fail closed instead of
     * installing another hook over resident Common FPS code.
     */
    const int sentinel_socket = open_injection_sentinel();
    if (sentinel_socket < 0)
        return 0;

    (void)sentinel_socket; // kept open intentionally for SceShellUI lifetime

    /*
     * Keep all network receiving off the ShellUI main thread.  The visual
     * bootstrap below only installs a callback; actual PUI changes happen
     * from Application.Update.
     */
    if (!initialize_receiver())
        return 1;

    const int health_socket = open_health_sender();
    std::uint64_t health_sequence = 1;

    unsigned ticks_until_health = 0;
    unsigned ticks_until_bootstrap = 0;
    bool hook_installed = false;

    for (;;) {
        if (!hook_installed && ticks_until_bootstrap == 0) {
            hook_installed = initialize_visual_hook();
            ticks_until_bootstrap = 16; // ~256 ms retry, never a busy loop.
        }

        if (ticks_until_bootstrap > 0)
            --ticks_until_bootstrap;

        if (ticks_until_health == 0) {
            const auto phase =
                visual_ready()
                    ? common_fps::RendererHealthPhase::VisualReady
                    : common_fps::RendererHealthPhase::ReceiverReady;

            send_health(health_socket, health_sequence++, phase);
            ticks_until_health = 16;
        }

        --ticks_until_health;
        usleep(16000);
    }

    return 0;
}
