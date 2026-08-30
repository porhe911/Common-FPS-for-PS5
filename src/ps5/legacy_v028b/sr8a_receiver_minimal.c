/*
 * Common FPS v0.28b SR8A - minimal receiver-only ShellUI payload
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Intentionally tiny and UI-free. Imports are restricted to the same POSIX
 * primitives already present in the stable v0.28b embedded renderer.
 */

#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PHUF_MAGIC 0x46554850u
#define PHUF_VERSION 1u
#define PHUF_PORT_NETWORK 0xF5D8u /* htons(55541) on target */
#define LOOPBACK_ADDRESS 0x0100007fu
#define PHUF_TEXT_SIZE 1024u
#define HEALTH_PATH "/data/CommonFPS_SR8A_receiver_health.log"

struct phuf_packet {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    double fps;
    uint64_t reserved;
    char text[PHUF_TEXT_SIZE];
};

_Static_assert(sizeof(struct phuf_packet) == 0x420, "PHUF wire size");

static size_t append_u64(char* out, size_t pos, uint64_t value) {
    char temp[24];
    size_t n = 0;
    if (value == 0) {
        out[pos++] = '0';
        return pos;
    }
    while (value != 0 && n < sizeof(temp)) {
        temp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n != 0)
        out[pos++] = temp[--n];
    return pos;
}

static void write_packet_health(int fd, const struct phuf_packet* packet) {
    char line[80];
    size_t p = 0;
    const int loading = (packet->fps == 0.0 && packet->text[0] != '\0');
    line[p++] = loading ? 'L' : 'F';
    line[p++] = ' ';
    p = append_u64(line, p, packet->sequence);
    if (!loading) {
        const uint64_t tenths = (uint64_t)(packet->fps * 10.0 + 0.5);
        line[p++] = ' ';
        p = append_u64(line, p, tenths);
    }
    line[p++] = '\n';
    (void)write(fd, line, p);
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

    const int health = open(HEALTH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (health < 0) {
        close(receiver);
        return 12;
    }
    (void)write(health, "READY\n", 6);

    uint64_t last_sequence = 0;
    for (;;) {
        struct phuf_packet packet;
        const ssize_t got = recv(receiver, &packet, sizeof(packet), 0);
        if (got != (ssize_t)sizeof(packet))
            continue;
        if (packet.magic != PHUF_MAGIC || packet.version != PHUF_VERSION)
            continue;
        if (packet.sequence == 0 || packet.sequence <= last_sequence)
            continue;

        last_sequence = packet.sequence;
        write_packet_health(health, &packet);
    }
}
