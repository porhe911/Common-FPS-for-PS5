/*
 * Common FPS v0.28b stable-source rebuild - SR7 PHUF loopback transport probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Validates the recovered production backend together with the stable PHUF
 * UDP wire contract, without ShellUI injection or renderer code.  Every state
 * packet is sent to 127.0.0.1:55541 and received back by a local loopback
 * socket in this worker for byte/sequence/state validation.
 */

#include "phuf_wire.hpp"
#include "process_sysctl.hpp"
#include "videoout_counter.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

using common_fps::legacy_v028b::PhufStatePacket;
using common_fps::legacy_v028b::kPhufMagic;
using common_fps::legacy_v028b::kPhufVersion;

constexpr const char* kLogPath = "/data/CommonFPS_SR7_phuf_loopback.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint32_t kMaxTenthsFps = 1300U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;
constexpr unsigned kWarmupDeltasToDiscard = 1U;
constexpr std::uint16_t kNetworkPortLiteral = 0xF5D8U; // htons(55541) on x86-64
constexpr std::uint32_t kLoopbackLiteral = 0x0100007FU; // 127.0.0.1 in target memory
constexpr char kLoadingText[] = "FPS\tloading\n";

void log_line(const char* text) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, pid_t pid) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s%d\n", prefix, static_cast<int>(pid));
    std::fclose(f);
}

void log_counter(pid_t pid, std::uintptr_t address) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR7 COUNTER READY pid=%d address=0x%llx\n",
                 static_cast<int>(pid),
                 static_cast<unsigned long long>(address));
    std::fclose(f);
}

void log_rx_loading(std::uint64_t sequence) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR7 RX loading seq=%llu\n",
                 static_cast<unsigned long long>(sequence));
    std::fclose(f);
}

void log_rx_fps(std::uint64_t sequence, double fps) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR7 RX fps seq=%llu value=%.1f\n",
                 static_cast<unsigned long long>(sequence), fps);
    std::fclose(f);
}

void log_summary(std::uint64_t sent, std::uint64_t received,
                 std::uint64_t malformed, std::uint64_t missed) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f,
                 "SR7 SUMMARY sent=%llu received=%llu malformed=%llu missed=%llu\n",
                 static_cast<unsigned long long>(sent),
                 static_cast<unsigned long long>(received),
                 static_cast<unsigned long long>(malformed),
                 static_cast<unsigned long long>(missed));
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

class LoopbackTransport {
public:
    bool initialize() noexcept {
        receiver_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (receiver_ < 0) {
            log_line("SR7 UDP receiver socket failed");
            return false;
        }

        int reuse = 1;
        (void)setsockopt(receiver_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = kNetworkPortLiteral;
        local.sin_addr.s_addr = kLoopbackLiteral;
        if (bind(receiver_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
            log_line("SR7 UDP bind 127.0.0.1:55541 failed");
            close(receiver_);
            receiver_ = -1;
            return false;
        }

        sender_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sender_ < 0) {
            log_line("SR7 UDP sender socket failed");
            close(receiver_);
            receiver_ = -1;
            return false;
        }

        destination_ = local;
        log_line("SR7 UDP loopback ready 127.0.0.1:55541 packet_size=0x420");
        return true;
    }

    ~LoopbackTransport() {
        if (sender_ >= 0) close(sender_);
        if (receiver_ >= 0) close(receiver_);
    }

    void publish_loading() noexcept {
        PhufStatePacket packet{};
        packet.magic = kPhufMagic;
        packet.version = kPhufVersion;
        packet.sequence = next_sequence_++;
        packet.fps = 0.0;
        std::memcpy(packet.text, kLoadingText, sizeof(kLoadingText));
        send_and_validate(packet, true);
    }

    void publish_fps(double fps) noexcept {
        PhufStatePacket packet{};
        packet.magic = kPhufMagic;
        packet.version = kPhufVersion;
        packet.sequence = next_sequence_++;
        packet.fps = fps;
        packet.text[0] = '\0';
        send_and_validate(packet, false);
    }

    void summary() const noexcept {
        log_summary(sent_, received_, malformed_, missed_);
    }

private:
    void send_and_validate(const PhufStatePacket& packet, bool expect_loading) noexcept {
        const ssize_t sent = sendto(
            sender_, &packet, sizeof(packet), 0,
            reinterpret_cast<const sockaddr*>(&destination_), sizeof(destination_));
        if (sent != static_cast<ssize_t>(sizeof(packet))) {
            ++missed_;
            log_line("SR7 UDP send size/error");
            return;
        }
        ++sent_;

        PhufStatePacket received{};
        bool got_packet = false;
        for (unsigned attempt = 0; attempt < 20; ++attempt) {
            const ssize_t got = recvfrom(receiver_, &received, sizeof(received), MSG_DONTWAIT,
                                         nullptr, nullptr);
            if (got == static_cast<ssize_t>(sizeof(received))) {
                got_packet = true;
                break;
            }
            if (got >= 0) {
                ++malformed_;
                log_line("SR7 RX wrong packet size");
                return;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ++missed_;
                log_line("SR7 RX recvfrom error");
                return;
            }
            usleep(5000);
        }

        if (!got_packet) {
            ++missed_;
            log_line("SR7 RX timeout/missed packet");
            return;
        }
        ++received_;

        const bool header_ok =
            received.magic == kPhufMagic &&
            received.version == kPhufVersion &&
            received.sequence == packet.sequence;
        const bool loading_ok =
            received.fps == 0.0 &&
            std::memcmp(received.text, kLoadingText, sizeof(kLoadingText)) == 0;
        const bool fps_ok =
            received.fps > 0.0 && received.text[0] == '\0';

        if (!header_ok || (expect_loading ? !loading_ok : !fps_ok)) {
            ++malformed_;
            log_line("SR7 RX malformed PHUF packet");
            return;
        }

        if (expect_loading)
            log_rx_loading(received.sequence);
        else
            log_rx_fps(received.sequence, received.fps);
    }

    int receiver_ = -1;
    int sender_ = -1;
    sockaddr_in destination_{};
    std::uint64_t next_sequence_ = 1;
    std::uint64_t sent_ = 0;
    std::uint64_t received_ = 0;
    std::uint64_t malformed_ = 0;
    std::uint64_t missed_ = 0;
};

int run_probe() noexcept {
    using common_fps::legacy_v028b::find_game_pid_sysctl;
    using common_fps::legacy_v028b::read_videoout_counter;
    using common_fps::legacy_v028b::resolve_videoout_counter;

    log_pid("SR7 CHILD pid=", getpid());
    log_line("SR7 START production backend + PHUF loopback validation");
    log_line("SR7 NO ptrace / NO MDBG / NO renderer / NO ShellUI inject");
    log_line("SR7 wire=PHUF v1 size=0x420 udp=127.0.0.1:55541");

    LoopbackTransport transport;
    if (!transport.initialize()) {
        log_line("SR7 ABORT transport init failed");
        return 1;
    }

    pid_t active_pid = -1;
    std::uintptr_t counter_address = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool have_baseline = false;
    unsigned warmup_deltas = kWarmupDeltasToDiscard;
    int seconds_on_pid = 0;
    int retry_countdown = 0;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        const pid_t current_pid = find_game_pid_sysctl();
        if (current_pid != active_pid) {
            active_pid = current_pid;
            counter_address = 0;
            previous_counter = 0;
            previous_time = {};
            have_baseline = false;
            warmup_deltas = kWarmupDeltasToDiscard;
            seconds_on_pid = 0;
            retry_countdown = 0;
            log_pid("SR7 CHANGE eboot.bin pid=", active_pid);
        }

        if (active_pid <= 0) {
            transport.publish_loading();
            sleep(1);
            continue;
        }
        ++seconds_on_pid;

        if (counter_address == 0) {
            if (seconds_on_pid < kDiscoveryDelaySeconds) {
                transport.publish_loading();
                sleep(1);
                continue;
            }
            if (retry_countdown > 0) {
                --retry_countdown;
                transport.publish_loading();
                sleep(1);
                continue;
            }

            log_line("SR7 DISCOVERY TRY shared production backend");
            const auto resolved = resolve_videoout_counter(active_pid);
            if (!resolved) {
                log_line("SR7 DISCOVERY not ready; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                transport.publish_loading();
                sleep(1);
                continue;
            }

            counter_address = *resolved;
            have_baseline = false;
            warmup_deltas = kWarmupDeltasToDiscard;
            log_counter(active_pid, counter_address);
        }

        std::uint32_t current_counter = 0;
        if (!read_videoout_counter(active_pid, counter_address, current_counter)) {
            log_line("SR7 COUNTER read failed; drop and rediscover");
            counter_address = 0;
            have_baseline = false;
            warmup_deltas = kWarmupDeltasToDiscard;
            retry_countdown = kDiscoveryRetrySeconds;
            transport.publish_loading();
            sleep(1);
            continue;
        }

        timeval now{};
        gettimeofday(&now, nullptr);
        if (!have_baseline) {
            previous_counter = current_counter;
            previous_time = now;
            have_baseline = true;
            log_line("SR7 WARMUP baseline captured");
            transport.publish_loading();
            sleep(1);
            continue;
        }

        const std::int64_t elapsed = elapsed_us(previous_time, now);
        if (elapsed > 0) {
            const std::uint32_t delta =
                static_cast<std::uint32_t>(current_counter - previous_counter);
            const std::uint64_t scaled =
                static_cast<std::uint64_t>(delta) * kTenthsScale;
            const std::uint32_t tenths = static_cast<std::uint32_t>(
                scaled / static_cast<std::uint64_t>(elapsed));

            if (warmup_deltas != 0) {
                --warmup_deltas;
                log_line("SR7 WARMUP first delta discarded");
                transport.publish_loading();
            } else if (tenths > kMaxTenthsFps) {
                log_line("SR7 FPS sanity reject >130.0; publish loading, keep counter");
                transport.publish_loading();
            } else {
                transport.publish_fps(static_cast<double>(tenths) / 10.0);
            }
        } else {
            transport.publish_loading();
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    transport.summary();
    log_line("SR7 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR7 PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR7 PARENT forked child=", child);
        log_line("SR7 PARENT RETURN 0");
        return 0;
    }
    if (child < 0)
        log_line("SR7 fork failed; run in current process");
    return run_probe();
}
