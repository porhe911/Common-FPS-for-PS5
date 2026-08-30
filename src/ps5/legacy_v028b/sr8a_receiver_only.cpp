/*
 * Common FPS v0.28b stable-source rebuild - SR8A receiver-only ShellUI payload
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Deliberately NO Mono/PUI/UI hooks.  This injected payload only binds the
 * recovered PHUF endpoint and reports health/received sequence numbers back to
 * the controller over a separate local UDP port.
 */

#include "phuf_wire.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
using common_fps::legacy_v028b::PhufStatePacket;
using common_fps::legacy_v028b::kPhufMagic;
using common_fps::legacy_v028b::kPhufVersion;

constexpr std::uint16_t kPhufPortNetwork = 0xF5D8U;   // htons(55541)
constexpr std::uint16_t kHealthPortNetwork = 0xF6D8U; // htons(55542)
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kHealthMagic = 0x41384852U; // "RH8A" in memory
constexpr std::uint32_t kHealthReady = 1U;
constexpr std::uint32_t kHealthPacket = 2U;

struct HealthPacket {
    std::uint32_t magic;
    std::uint32_t kind;
    std::uint64_t sequence;
    double fps;
    std::uint32_t loading;
    std::uint32_t reserved;
};
static_assert(sizeof(HealthPacket) == 0x20);

bool send_health(int sock, const sockaddr_in& dst, std::uint32_t kind,
                 std::uint64_t sequence, double fps, bool loading) noexcept {
    HealthPacket h{};
    h.magic = kHealthMagic;
    h.kind = kind;
    h.sequence = sequence;
    h.fps = fps;
    h.loading = loading ? 1U : 0U;
    return sendto(sock, &h, sizeof(h), 0,
                  reinterpret_cast<const sockaddr*>(&dst), sizeof(dst)) ==
           static_cast<ssize_t>(sizeof(h));
}
}

extern "C" int main() {
    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0)
        return 10;

    int reuse = 1;
    (void)setsockopt(receiver, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = kPhufPortNetwork;
    local.sin_addr.s_addr = kLoopback;
    if (bind(receiver, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
        close(receiver);
        return 11;
    }

    const int health = socket(AF_INET, SOCK_DGRAM, 0);
    if (health < 0) {
        close(receiver);
        return 12;
    }

    sockaddr_in health_dst{};
    health_dst.sin_family = AF_INET;
    health_dst.sin_port = kHealthPortNetwork;
    health_dst.sin_addr.s_addr = kLoopback;

    (void)send_health(health, health_dst, kHealthReady, 0, 0.0, true);

    std::uint64_t last_sequence = 0;
    for (;;) {
        PhufStatePacket packet{};
        const ssize_t got = recvfrom(receiver, &packet, sizeof(packet), 0, nullptr, nullptr);
        if (got != static_cast<ssize_t>(sizeof(packet))) {
            if (got < 0 && errno == EINTR)
                continue;
            usleep(10000);
            continue;
        }

        if (packet.magic != kPhufMagic || packet.version != kPhufVersion)
            continue;
        if (packet.sequence == 0 || packet.sequence <= last_sequence)
            continue;

        last_sequence = packet.sequence;
        const bool loading = packet.fps == 0.0 && packet.text[0] != '\0';
        (void)send_health(health, health_dst, kHealthPacket,
                          packet.sequence, packet.fps, loading);
    }
}
