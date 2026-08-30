/*
 * Common FPS v0.28b SR8A - fail-closed minimal receiver bootstrap probe
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "phuf_wire.hpp"
#include "process_sysctl.hpp"
#include "stable_injector.hpp"
#include "videoout_counter.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
extern const unsigned char commonfps_sr8a_receiver_elf[];
extern const std::size_t commonfps_sr8a_receiver_elf_size;
int commonfps_v028b_trace_continue_seen(void);
int commonfps_v028b_trace_stop_seen(void);
unsigned commonfps_v028b_import_resolved_count(void);
unsigned commonfps_v028b_import_unresolved_count(void);
const char* commonfps_v028b_first_unresolved(void);
}

namespace {
using common_fps::legacy_v028b::PhufStatePacket;
using common_fps::legacy_v028b::kPhufMagic;
using common_fps::legacy_v028b::kPhufVersion;

constexpr const char* kLogPath = "/data/CommonFPS_SR8A_receiver_bootstrap.log";
constexpr const char* kHealthPath = "/data/CommonFPS_SR8A_receiver_health.log";
constexpr int kRuntimeSeconds = 240;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint16_t kPhufPortNetwork = 0xF5D8U;
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kMaxTenthsFps = 1300U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;
constexpr char kLoadingText[] = "FPS\tloading\n";

void log_line(const char* s) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s\n", s);
    std::fclose(f);
}

void log_pid(const char* tag, pid_t pid) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s%d\n", tag, static_cast<int>(pid));
    std::fclose(f);
}

void log_injection(const common_fps::legacy_v028b::InjectionResult& r) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    const char* unresolved = commonfps_v028b_first_unresolved();
    std::fprintf(f,
        "SR8A INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",
        r.attached ? 1 : 0, r.elf_loaded ? 1 : 0,
        r.payload_args_ready ? 1 : 0, r.bootstrap_started ? 1 : 0,
        r.pthread_create_ok ? 1 : 0, r.pthread_create_rc,
        commonfps_v028b_trace_continue_seen(), commonfps_v028b_trace_stop_seen(),
        commonfps_v028b_import_resolved_count(),
        commonfps_v028b_import_unresolved_count(),
        (unresolved && *unresolved) ? unresolved : "none");
    std::fclose(f);
}

bool health_ready() noexcept {
    FILE* f = std::fopen(kHealthPath, "r");
    if (!f) return false;
    char line[16]{};
    const bool ok = std::fgets(line, sizeof(line), f) != nullptr &&
                    std::strncmp(line, "READY", 5) == 0;
    std::fclose(f);
    return ok;
}

bool wait_health(unsigned milliseconds) noexcept {
    for (unsigned elapsed = 0; elapsed < milliseconds; elapsed += 50) {
        if (health_ready()) return true;
        usleep(50000);
    }
    return false;
}

std::int64_t elapsed_us(const timeval& a, const timeval& b) noexcept {
    return static_cast<std::int64_t>(b.tv_sec - a.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(b.tv_usec - a.tv_usec);
}

class PhufSender {
public:
    bool initialize() noexcept {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) return false;
        destination_.sin_family = AF_INET;
        destination_.sin_port = kPhufPortNetwork;
        destination_.sin_addr.s_addr = kLoopback;
        return true;
    }

    ~PhufSender() {
        if (sock_ >= 0) close(sock_);
    }

    void loading() noexcept {
        PhufStatePacket p{};
        p.magic = kPhufMagic;
        p.version = kPhufVersion;
        p.sequence = next_++;
        p.fps = 0.0;
        std::memcpy(p.text, kLoadingText, sizeof(kLoadingText));
        send(p);
    }

    void fps(double value) noexcept {
        PhufStatePacket p{};
        p.magic = kPhufMagic;
        p.version = kPhufVersion;
        p.sequence = next_++;
        p.fps = value;
        send(p);
    }

private:
    void send(const PhufStatePacket& p) noexcept {
        const ssize_t n = sendto(sock_, &p, sizeof(p), 0,
            reinterpret_cast<const sockaddr*>(&destination_), sizeof(destination_));
        if (n != static_cast<ssize_t>(sizeof(p)))
            log_line("SR8A PHUF send failed");
    }

    int sock_ = -1;
    sockaddr_in destination_{};
    std::uint64_t next_ = 1;
};

pid_t wait_stable_shellui() noexcept {
    using common_fps::legacy_v028b::find_process_pid_sysctl;
    pid_t previous = -1;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const pid_t now = find_process_pid_sysctl("SceShellUI");
        if (now > 0 && now == previous)
            return now;
        previous = now;
        sleep(2);
    }
    return -1;
}

int run_probe() noexcept {
    using common_fps::legacy_v028b::find_game_pid_sysctl;
    using common_fps::legacy_v028b::inject_renderer_once;
    using common_fps::legacy_v028b::read_videoout_counter;
    using common_fps::legacy_v028b::resolve_videoout_counter;

    log_pid("SR8A CHILD pid=", getpid());
    log_line("SR8A START fail-closed receiver-only ShellUI bootstrap");
    log_line("SR8A NO PUI / NO Mono / NO UI hook / one injection only");
    log_line("SR8A loader=libNineS+TraceContinue+PHU GLOB_DAT resolver");

    (void)unlink(kHealthPath);

    const pid_t shellui = wait_stable_shellui();
    log_pid("SR8A ShellUI stable pid=", shellui);
    if (shellui <= 0) return 2;

    const auto injection = inject_renderer_once(
        shellui, commonfps_sr8a_receiver_elf, commonfps_sr8a_receiver_elf_size);
    log_injection(injection);

    if (commonfps_v028b_import_unresolved_count() != 0) {
        log_line("SR8A ABORT fail-closed import resolver rejected receiver");
        return 3;
    }
    if (!injection.pthread_create_ok) {
        log_line("SR8A ABORT injection did not reach successful pthread_create");
        return 4;
    }
    if (!wait_health(10000)) {
        log_line("SR8A ABORT no receiver READY file heartbeat");
        return 5;
    }

    log_line("SR8A RECEIVER READY PASS; begin production PHUF lifecycle");

    PhufSender wire;
    if (!wire.initialize()) {
        log_line("SR8A ABORT PHUF sender socket failed");
        return 6;
    }

    pid_t active_pid = -1;
    std::uintptr_t counter = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool baseline = false;
    unsigned warmup = 1;
    int seconds_on_pid = 0;
    int retry = 0;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        const pid_t pid = find_game_pid_sysctl();
        if (pid != active_pid) {
            active_pid = pid;
            counter = 0;
            baseline = false;
            warmup = 1;
            seconds_on_pid = 0;
            retry = 0;
            log_pid("SR8A CHANGE eboot.bin pid=", active_pid);
        }

        if (active_pid <= 0) {
            wire.loading();
            sleep(1);
            continue;
        }
        ++seconds_on_pid;

        if (counter == 0) {
            if (seconds_on_pid < kDiscoveryDelaySeconds) {
                wire.loading();
                sleep(1);
                continue;
            }
            if (retry > 0) {
                --retry;
                wire.loading();
                sleep(1);
                continue;
            }
            const auto resolved = resolve_videoout_counter(active_pid);
            if (!resolved) {
                log_line("SR8A DISCOVERY not ready; retry in 5s");
                retry = kDiscoveryRetrySeconds;
                wire.loading();
                sleep(1);
                continue;
            }
            counter = *resolved;
            baseline = false;
            warmup = 1;
            log_line("SR8A COUNTER READY");
        }

        std::uint32_t current = 0;
        if (!read_videoout_counter(active_pid, counter, current)) {
            counter = 0;
            baseline = false;
            retry = kDiscoveryRetrySeconds;
            wire.loading();
            sleep(1);
            continue;
        }

        timeval now{};
        gettimeofday(&now, nullptr);
        if (!baseline) {
            previous_counter = current;
            previous_time = now;
            baseline = true;
            wire.loading();
            sleep(1);
            continue;
        }

        const auto elapsed = elapsed_us(previous_time, now);
        if (elapsed > 0) {
            const std::uint32_t delta =
                static_cast<std::uint32_t>(current - previous_counter);
            const std::uint32_t tenths = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(delta) * kTenthsScale /
                static_cast<std::uint64_t>(elapsed));
            if (warmup) {
                --warmup;
                wire.loading();
            } else if (tenths > kMaxTenthsFps) {
                wire.loading();
            } else {
                wire.fps(static_cast<double>(tenths) / 10.0);
            }
        } else {
            wire.loading();
        }

        previous_counter = current;
        previous_time = now;
        sleep(1);
    }

    log_line("SR8A DONE controller clean return after 240s; receiver remains until reboot");
    return 0;
}
}

extern "C" int main() {
    log_pid("SR8A PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR8A PARENT forked child=", child);
        log_line("SR8A PARENT RETURN 0");
        return 0;
    }
    if (child < 0)
        log_line("SR8A fork failed; run current process");
    return run_probe();
}
