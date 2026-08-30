/*
 * Common FPS v0.28b SR8B - minimal receiver-only ShellUI payload
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * No PUI, Mono, UI hooks, files, or game access.  The payload binds the
 * hardware-proven PHUF endpoint 127.0.0.1:55541 and reports READY plus every
 * accepted PHUF packet back to the controller on 127.0.0.1:55542.
 */

#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PHUF_MAGIC 0x46554850u
#define PHUF_VERSION 1u
#define PHUF_PORT_NETWORK 0xF5D8u   /* htons(55541) on target */
#define HEALTH_PORT_NETWORK 0xF6D8u /* htons(55542) on target */
#define LOOPBACK_ADDRESS 0x0100007fu
#define PHUF_TEXT_SIZE 1024u
#define HEALTH_MAGIC 0x42384852u /* "RH8B" in memory */
#define HEALTH_READY 1u
#define HEALTH_PACKET 2u

struct phuf_packet {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    double fps;
    uint64_t reserved;
    char text[PHUF_TEXT_SIZE];
};

struct health_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    double fps;
    uint32_t loading;
    uint32_t reserved;
};

_Static_assert(sizeof(struct phuf_packet) == 0x420, "PHUF wire size");
_Static_assert(sizeof(struct health_packet) == 0x20, "health wire size");

static void send_health(int sock, const struct sockaddr_in* dst,
                        uint32_t kind, uint64_t sequence,
                        double fps, uint32_t loading) {
    struct health_packet h;
    memset(&h, 0, sizeof(h));
    h.magic = HEALTH_MAGIC;
    h.kind = kind;
    h.sequence = sequence;
    h.fps = fps;
    h.loading = loading;
    (void)sendto(sock, &h, sizeof(h), 0,
                 (const struct sockaddr*)dst, sizeof(*dst));
}

int main(void) {
    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0)
        return 10;

    int reuse = 1;
    (void)setsockopt(receiver, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = PHUF_PORT_NETWORK;
    local.sin_addr.s_addr = LOOPBACK_ADDRESS;
    if (bind(receiver, (const struct sockaddr*)&local, sizeof(local)) != 0) {
        close(receiver);
        return 11;
    }

    const int health = socket(AF_INET, SOCK_DGRAM, 0);
    if (health < 0) {
        close(receiver);
        return 12;
    }

    struct sockaddr_in health_dst;
    memset(&health_dst, 0, sizeof(health_dst));
    health_dst.sin_family = AF_INET;
    health_dst.sin_port = HEALTH_PORT_NETWORK;
    health_dst.sin_addr.s_addr = LOOPBACK_ADDRESS;

    send_health(health, &health_dst, HEALTH_READY, 0, 0.0, 1);

    uint64_t last_sequence = 0;
    for (;;) {
        struct phuf_packet packet;
        const ssize_t got = recv(receiver, &packet, sizeof(packet), 0);
        if (got != (ssize_t)sizeof(packet)) {
            usleep(10000);
            continue;
        }
        if (packet.magic != PHUF_MAGIC || packet.version != PHUF_VERSION)
            continue;
        if (packet.sequence == 0 || packet.sequence <= last_sequence)
            continue;

        last_sequence = packet.sequence;
        const uint32_t loading =
            (packet.fps == 0.0 && packet.text[0] != '\0') ? 1u : 0u;
        send_health(health, &health_dst, HEALTH_PACKET,
                    packet.sequence, packet.fps, loading);
    }
}
