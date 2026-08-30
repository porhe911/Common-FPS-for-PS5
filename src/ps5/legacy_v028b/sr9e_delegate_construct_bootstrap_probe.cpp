/*
 * Common FPS v0.28b SR9E - System.Action construction-only controller
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
extern const unsigned char commonfps_sr9e_receiver_elf[];
extern const std::size_t commonfps_sr9e_receiver_elf_size;
int commonfps_v028b_trace_continue_seen(void);
int commonfps_v028b_trace_stop_seen(void);
unsigned commonfps_v028b_import_resolved_count(void);
unsigned commonfps_v028b_import_unresolved_count(void);
const char* commonfps_v028b_first_unresolved(void);
}

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_SR9E_delegate_construct.log";
constexpr std::uint16_t kPortNetwork = 0xF9D8U; // htons(55545)
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kMagic = 0x45394544U;
constexpr std::uint32_t kKindStage = 1U;
constexpr std::uint32_t kKindResult = 2U;
constexpr std::uint32_t kKindDone = 3U;
constexpr std::uint32_t kKindError = 4U;

struct ResultPacket {
    std::uint32_t magic;
    std::uint32_t kind;
    std::uint64_t sequence;
    std::uint32_t code;
    std::uint32_t reserved;
    char detail[96];
    char extra[160];
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
        "SR9E INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",
        r.attached ? 1 : 0, r.elf_loaded ? 1 : 0,
        r.payload_args_ready ? 1 : 0, r.bootstrap_started ? 1 : 0,
        r.pthread_create_ok ? 1 : 0, r.pthread_create_rc,
        commonfps_v028b_trace_continue_seen(), commonfps_v028b_trace_stop_seen(),
        commonfps_v028b_import_resolved_count(),
        commonfps_v028b_import_unresolved_count(),
        (unresolved && *unresolved) ? unresolved : "none");
    std::fclose(f);
}

void log_packet(const ResultPacket& p) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    if (p.kind == kKindStage) {
        std::fprintf(f, "SR9E STAGE seq=%llu detail=%s extra=%s\n",
                     static_cast<unsigned long long>(p.sequence), p.detail, p.extra);
    } else if (p.kind == kKindResult) {
        std::fprintf(f, "SR9E RESULT code=%u detail=%s extra=%s\n",
                     p.code, p.detail, p.extra);
    } else if (p.kind == kKindDone) {
        std::fprintf(f, "SR9E DONE detail=%s extra=%s\n", p.detail, p.extra);
    } else if (p.kind == kKindError) {
        std::fprintf(f, "SR9E ERROR code=%u detail=%s extra=%s\n",
                     p.code, p.detail, p.extra);
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

    log_pid("SR9E CHILD pid=", getpid());
    log_line("SR9E START System.Action construction-only probe");
    log_line("SR9E NO EnqueueEventAction / NO delegate invoke / NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch");
    log_line("SR9E target: construct System.Action bound to RequestSwapBuffersIfNoDraw and verify type only");

    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0) return 1;
    int reuse = 1;
    (void)setsockopt(receiver, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = kPortNetwork;
    local.sin_addr.s_addr = kLoopback;
    if (bind(receiver, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
        close(receiver);
        return 2;
    }

    const pid_t shellui = wait_stable_shellui();
    log_pid("SR9E ShellUI stable pid=", shellui);
    if (shellui <= 0) {
        close(receiver);
        return 3;
    }

    const auto injection = inject_renderer_once(
        shellui, commonfps_sr9e_receiver_elf, commonfps_sr9e_receiver_elf_size);
    log_injection(injection);
    if (commonfps_v028b_import_unresolved_count() != 0 || !injection.pthread_create_ok) {
        close(receiver);
        return 4;
    }

    bool done = false;
    bool error = false;
    for (unsigned elapsed_ms = 0; elapsed_ms < 8000; elapsed_ms += 10) {
        ResultPacket p{};
        const ssize_t got = recvfrom(receiver, &p, sizeof(p), MSG_DONTWAIT, nullptr, nullptr);
        if (got == static_cast<ssize_t>(sizeof(p)) && p.magic == kMagic) {
            log_packet(p);
            if (p.kind == kKindDone) { done = true; break; }
            if (p.kind == kKindError) { error = true; break; }
        }
        usleep(10000);
    }

    close(receiver);
    if (error) {
        log_line("SR9E FAIL receiver reported delegate-construction error");
        return 5;
    }
    if (!done) {
        log_line("SR9E FAIL delegate-construction timeout");
        return 6;
    }

    log_line("SR9E PASS System.Action constructed and verified; delegate was NOT invoked or enqueued");
    return 0;
}
}

extern "C" int main() {
    log_pid("SR9E PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR9E PARENT forked child=", child);
        log_line("SR9E PARENT RETURN 0");
        return 0;
    }
    if (child < 0) log_line("SR9E fork failed; run current process");
    return run_probe();
}
