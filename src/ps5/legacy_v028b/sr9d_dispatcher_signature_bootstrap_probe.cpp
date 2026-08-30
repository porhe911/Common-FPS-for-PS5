/*
 * Common FPS v0.28b SR9D - targeted dispatcher signature controller
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
extern const unsigned char commonfps_sr9d_receiver_elf[];
extern const std::size_t commonfps_sr9d_receiver_elf_size;
int commonfps_v028b_trace_continue_seen(void);
int commonfps_v028b_trace_stop_seen(void);
unsigned commonfps_v028b_import_resolved_count(void);
unsigned commonfps_v028b_import_unresolved_count(void);
const char* commonfps_v028b_first_unresolved(void);
}

namespace {
constexpr const char* kLogPath = "/data/CommonFPS_SR9D_signature_probe.log";
constexpr std::uint16_t kPortNetwork = 0xF8D8U; // htons(55544)
constexpr std::uint32_t kLoopback = 0x0100007FU;
constexpr std::uint32_t kMagic = 0x44394453U;
constexpr std::uint32_t kKindStage = 1U;
constexpr std::uint32_t kKindSignature = 2U;
constexpr std::uint32_t kKindDone = 3U;
constexpr std::uint32_t kKindError = 4U;
constexpr std::uint32_t kMethodStatic = 0x0010U;

struct SignaturePacket {
    std::uint32_t magic;
    std::uint32_t kind;
    std::uint64_t sequence;
    std::uint32_t flags;
    std::uint32_t param_count;
    char method_name[64];
    char return_type[96];
    char param_types[256];
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
        "SR9D INJECT attached=%d elf_loaded=%d payload_args=%d bootstrap=%d pthread_ok=%d pthread_rc=%d trace_continue=%d trace_stop=%d imports_resolved=%u imports_unresolved=%u first_unresolved=%s\n",
        r.attached ? 1 : 0, r.elf_loaded ? 1 : 0,
        r.payload_args_ready ? 1 : 0, r.bootstrap_started ? 1 : 0,
        r.pthread_create_ok ? 1 : 0, r.pthread_create_rc,
        commonfps_v028b_trace_continue_seen(), commonfps_v028b_trace_stop_seen(),
        commonfps_v028b_import_resolved_count(),
        commonfps_v028b_import_unresolved_count(),
        (unresolved && *unresolved) ? unresolved : "none");
    std::fclose(f);
}

void log_packet(const SignaturePacket& p) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    if (p.kind == kKindSignature) {
        std::fprintf(f,
            "SR9D SIGNATURE name=%s static=%u flags=0x%08x params=%u return=%s args=[%s]\n",
            p.method_name, (p.flags & kMethodStatic) ? 1U : 0U,
            p.flags, p.param_count,
            p.return_type[0] ? p.return_type : "?",
            p.param_types);
    } else if (p.kind == kKindStage) {
        std::fprintf(f, "SR9D STAGE seq=%llu detail=%s extra=%s\n",
                     static_cast<unsigned long long>(p.sequence),
                     p.method_name, p.return_type);
    } else if (p.kind == kKindDone) {
        std::fprintf(f, "SR9D DONE signatures=%u\n", p.param_count);
    } else if (p.kind == kKindError) {
        std::fprintf(f, "SR9D ERROR code=%u detail=%s extra=%s\n",
                     p.param_count, p.method_name, p.return_type);
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

    log_pid("SR9D CHILD pid=", getpid());
    log_line("SR9D START targeted PUI dispatcher signature probe");
    log_line("SR9D NO CreateLabel / NO AppendChild / NO PUI mutation / NO Detour / NO code patch");
    log_line("SR9D target: exact signatures only for EnqueueEventAction / FrameBegun / synchronization / render methods");

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
    log_pid("SR9D ShellUI stable pid=", shellui);
    if (shellui <= 0) {
        close(receiver);
        return 3;
    }

    const auto injection = inject_renderer_once(
        shellui, commonfps_sr9d_receiver_elf, commonfps_sr9d_receiver_elf_size);
    log_injection(injection);
    if (commonfps_v028b_import_unresolved_count() != 0 || !injection.pthread_create_ok) {
        close(receiver);
        return 4;
    }

    bool done = false;
    bool error = false;
    for (unsigned elapsed_ms = 0; elapsed_ms < 8000; elapsed_ms += 10) {
        SignaturePacket p{};
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
        log_line("SR9D FAIL receiver reported signature error");
        return 5;
    }
    if (!done) {
        log_line("SR9D FAIL signature timeout");
        return 6;
    }

    log_line("SR9D PASS signature capture complete; no UI mutation performed");
    return 0;
}
}

extern "C" int main() {
    log_pid("SR9D PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR9D PARENT forked child=", child);
        log_line("SR9D PARENT RETURN 0");
        return 0;
    }
    if (child < 0) log_line("SR9D fork failed; run current process");
    return run_probe();
}
