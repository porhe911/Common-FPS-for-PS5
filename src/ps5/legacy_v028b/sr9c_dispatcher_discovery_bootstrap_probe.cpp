/*
 * Common FPS v0.28b SR9C - metadata-only PUI dispatcher discovery controller
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "process_sysctl.hpp"
#include "stable_injector.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
extern const unsigned char commonfps_sr9c_receiver_elf[];
extern const std::size_t commonfps_sr9c_receiver_elf_size;
int commonfps_v028b_trace_continue_seen(void);
int commonfps_v028b_trace_stop_seen(void);
unsigned commonfps_v028b_import_resolved_count(void);
unsigned commonfps_v028b_import_unresolved_count(void);
const char* commonfps_v028b_first_unresolved(void);
}

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_SR9C_dispatcher_discovery.log";
constexpr std::uint16_t kDiscoveryPortNetwork = 0xF7D8U; // htons(55543)
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kDiscoveryMagic = 0x43394452U;
constexpr std::uint32_t kKindStage = 1U;
constexpr std::uint32_t kKindMethod = 2U;
constexpr std::uint32_t kKindDone = 3U;
constexpr std::uint32_t kKindError = 4U;

struct DiscoveryPacket {
    std::uint32_t magic;
    std::uint32_t kind;
    std::uint64_t sequence;
    std::uint32_t class_id;
    std::uint32_t param_count;
    char class_name[64];
    char method_name[96];
};

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
        "SR9C INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",
        r.attached ? 1 : 0, r.elf_loaded ? 1 : 0,
        r.payload_args_ready ? 1 : 0, r.bootstrap_started ? 1 : 0,
        r.pthread_create_ok ? 1 : 0, r.pthread_create_rc,
        commonfps_v028b_trace_continue_seen(), commonfps_v028b_trace_stop_seen(),
        commonfps_v028b_import_resolved_count(),
        commonfps_v028b_import_unresolved_count(),
        (unresolved && *unresolved) ? unresolved : "none");
    std::fclose(f);
}

void log_packet(const DiscoveryPacket& p) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    if (p.kind == kKindMethod) {
        std::fprintf(f, "SR9C METHOD class_id=%u class=%s name=%s params=%u\n",
                     p.class_id, p.class_name, p.method_name, p.param_count);
    } else if (p.kind == kKindStage) {
        std::fprintf(f, "SR9C STAGE seq=%llu class_id=%u class=%s detail=%s\n",
                     static_cast<unsigned long long>(p.sequence),
                     p.class_id, p.class_name, p.method_name);
    } else if (p.kind == kKindDone) {
        std::fprintf(f, "SR9C DONE reported_methods=%u\n", p.param_count);
    } else if (p.kind == kKindError) {
        std::fprintf(f, "SR9C ERROR code=%u class=%s detail=%s\n",
                     p.param_count, p.class_name, p.method_name);
    }
    std::fclose(f);
}

pid_t wait_stable_shellui() noexcept {
    using common_fps::legacy_v028b::find_process_pid_sysctl;
    pid_t previous = -1;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const pid_t now = find_process_pid_sysctl("SceShellUI");
        if (now > 0 && now == previous) return now;
        previous = now;
        sleep(2);
    }
    return -1;
}

int run_probe() noexcept {
    using common_fps::legacy_v028b::inject_renderer_once;

    log_pid("SR9C CHILD pid=", getpid());
    log_line("SR9C START metadata-only PUI dispatcher discovery");
    log_line("SR9C NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch");
    log_line("SR9C goal: discover existing UI/main-thread dispatcher before any new visual test");

    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0) {
        log_line("SR9C ABORT UDP socket failed");
        return 1;
    }
    int reuse = 1;
    (void)setsockopt(receiver, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = kDiscoveryPortNetwork;
    local.sin_addr.s_addr = kLoopback;
    if (bind(receiver, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
        log_line("SR9C ABORT UDP bind 127.0.0.1:55543 failed");
        close(receiver);
        return 2;
    }

    const pid_t shellui = wait_stable_shellui();
    log_pid("SR9C ShellUI stable pid=", shellui);
    if (shellui <= 0) {
        close(receiver);
        return 3;
    }

    const auto injection = inject_renderer_once(
        shellui, commonfps_sr9c_receiver_elf, commonfps_sr9c_receiver_elf_size);
    log_injection(injection);
    if (commonfps_v028b_import_unresolved_count() != 0) {
        log_line("SR9C ABORT fail-closed import resolver rejected receiver");
        close(receiver);
        return 4;
    }
    if (!injection.pthread_create_ok) {
        log_line("SR9C ABORT injection did not reach successful pthread_create");
        close(receiver);
        return 5;
    }

    bool done = false;
    bool error = false;
    for (unsigned elapsed_ms = 0; elapsed_ms < 15000; elapsed_ms += 10) {
        DiscoveryPacket p{};
        const ssize_t got = recvfrom(receiver, &p, sizeof(p), MSG_DONTWAIT, nullptr, nullptr);
        if (got == static_cast<ssize_t>(sizeof(p)) && p.magic == kDiscoveryMagic) {
            log_packet(p);
            if (p.kind == kKindDone) {
                done = true;
                break;
            }
            if (p.kind == kKindError) {
                error = true;
                break;
            }
        }
        usleep(10000);
    }

    close(receiver);
    if (error) {
        log_line("SR9C FAIL receiver reported metadata discovery error");
        return 6;
    }
    if (!done) {
        log_line("SR9C FAIL metadata discovery timeout");
        return 7;
    }

    log_line("SR9C PASS metadata discovery complete; inspect METHOD lines before choosing next UI-thread mechanism");
    return 0;
}
}

extern "C" int main() {
    log_pid("SR9C PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR9C PARENT forked child=", child);
        log_line("SR9C PARENT RETURN 0");
        return 0;
    }
    if (child < 0) log_line("SR9C fork failed; run current process");
    return run_probe();
}
