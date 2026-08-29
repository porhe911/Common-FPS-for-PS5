/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ps5_autoload_backend.hpp"

#include "common_fps/renderer_health.hpp"
#include "embedded_renderer.hpp"
#include "ps5_platform.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include "elfldr.h"
#include "proc.h"
#include "pt.h"
}

namespace common_fps::ps5 {
namespace {

constexpr std::uint64_t kHeartbeatFreshUs = 2'500'000ULL;
constexpr std::uint64_t kSamePidInjectionCooldownUs = 10'000'000ULL;
constexpr std::size_t kMaxRendererElfSize = 8U * 1024U * 1024U;

void diag_log(const char* text) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_controller.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void diag_log_i(const char* prefix, long long value) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_controller.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s%lld\n", prefix, value);
    std::fclose(f);
}

bool looks_like_elf(const unsigned char* data, std::size_t size) {
    return data != nullptr && size >= 4 &&
           data[0] == 0x7f && data[1] == 'E' &&
           data[2] == 'L' && data[3] == 'F';
}

} // namespace

Ps5AutoloadBackend::Ps5AutoloadBackend(Ps5Platform& platform)
    : platform_(platform) {
    open_health_socket();
}

Ps5AutoloadBackend::~Ps5AutoloadBackend() {
    if (health_socket_ >= 0)
        close(health_socket_);
}

bool Ps5AutoloadBackend::valid() const noexcept {
    return health_socket_ >= 0;
}

bool Ps5AutoloadBackend::open_health_socket() {
    health_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (health_socket_ < 0)
        return false;

    const int flags = fcntl(health_socket_, F_GETFL, 0);
    if (flags >= 0)
        fcntl(health_socket_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(common_fps::kRendererHealthPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(health_socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(health_socket_);
        health_socket_ = -1;
        return false;
    }
    diag_log("C3 health socket ready");
    return true;
}

void Ps5AutoloadBackend::reset_heartbeat_state(pid_t new_pid) {
    shellui_pid_ = new_pid;
    last_receiver_heartbeat_us_ = 0;
    last_visual_heartbeat_us_ = 0;
    last_health_sequence_ = 0;
    if (last_injected_pid_ != new_pid) {
        last_injected_pid_ = -1;
        last_injection_attempt_us_ = 0;
    }
}

bool Ps5AutoloadBackend::refresh_shellui() {
    static pid_t last_logged_pid = -2;
    struct proc* process = find_proc_by_name("SceShellUI");
    if (!process) {
        if (shellui_pid_ != -1)
            reset_heartbeat_state(-1);
        return false;
    }

    const pid_t pid = process->pid;
    std::free(process);
    if (pid != shellui_pid_)
        reset_heartbeat_state(pid);
    if (pid != last_logged_pid) {
        diag_log_i("C4 SceShellUI pid=", pid);
        last_logged_pid = pid;
    }
    return pid > 0;
}

bool Ps5AutoloadBackend::mono_runtime_present(pid_t pid) {
    if (!platform_.begin_process_inspection(pid)) {
        diag_log("C5 Mono check: begin_process_inspection FAIL");
        return false;
    }

    module_info_t* mono = get_module_info(pid, "libmonosgen-2.0.sprx");
    platform_.end_process_inspection(pid);

    if (!mono) {
        diag_log("C5 Mono check: libmonosgen not visible");
        return false;
    }
    std::free(mono);
    diag_log("C5 Mono check: present");
    return true;
}

bool Ps5AutoloadBackend::runtime_ready() {
    /*
     * Diagnostic RC3 deliberately does NOT gate injection on Mono module
     * enumeration. RC2 only logged C0, so it may never have reached the
     * injector. The smoke payload itself does not use Mono, hooks or widgets;
     * for this one hardware test, an alive SceShellUI is sufficient.
     */
    if (!valid())
        return false;
    if (!refresh_shellui())
        return false;
    static bool logged = false;
    if (!logged) {
        diag_log("C6 RC3 runtime ready: ShellUI only (Mono gate bypassed)");
        logged = true;
    }
    return true;
}

void Ps5AutoloadBackend::drain_health() {
    if (health_socket_ < 0)
        return;

    for (;;) {
        common_fps::RendererHealthPacket packet{};
        const ssize_t n = recv(health_socket_, &packet, sizeof(packet), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        if (n != static_cast<ssize_t>(sizeof(packet)) ||
            !common_fps::valid_renderer_health_packet(packet) ||
            packet.shellui_pid != shellui_pid_)
            continue;

        if (last_health_sequence_ != 0 && packet.sequence != 0 &&
            packet.sequence < last_health_sequence_)
            continue;

        const bool first = last_receiver_heartbeat_us_ == 0;
        last_health_sequence_ = packet.sequence;
        const auto now = platform_.monotonic_us();
        last_receiver_heartbeat_us_ = now;
        if (packet.phase == static_cast<std::uint16_t>(
                common_fps::RendererHealthPhase::VisualReady))
            last_visual_heartbeat_us_ = now;
        if (first)
            diag_log("C11 first renderer heartbeat received");
    }
}

bool Ps5AutoloadBackend::heartbeat_fresh(std::uint64_t timestamp_us) const {
    if (timestamp_us == 0)
        return false;
    const auto now = platform_.monotonic_us();
    return now >= timestamp_us && (now - timestamp_us) <= kHeartbeatFreshUs;
}

bool Ps5AutoloadBackend::renderer_alive() {
    if (!refresh_shellui())
        return false;
    drain_health();
    return heartbeat_fresh(last_receiver_heartbeat_us_);
}

bool Ps5AutoloadBackend::visual_ready() {
    if (!refresh_shellui())
        return false;
    drain_health();
    return heartbeat_fresh(last_visual_heartbeat_us_);
}

bool Ps5AutoloadBackend::read_renderer_elf(unsigned char** data, std::size_t* size) const {
    if (!data || !size)
        return false;
    *data = nullptr;
    *size = 0;

    const auto* bytes = embedded_renderer::kData;
    const std::size_t bytes_size = embedded_renderer::kSize;
    if (!looks_like_elf(bytes, bytes_size) ||
        bytes_size == 0 || bytes_size > kMaxRendererElfSize) {
        diag_log("C7 embedded renderer invalid");
        return false;
    }

    auto* copy = static_cast<unsigned char*>(std::malloc(bytes_size));
    if (!copy) {
        diag_log("C7 embedded renderer malloc FAIL");
        return false;
    }

    std::memcpy(copy, bytes, bytes_size);
    *data = copy;
    *size = bytes_size;
    diag_log_i("C7 embedded renderer bytes=", static_cast<long long>(bytes_size));
    return true;
}

bool Ps5AutoloadBackend::install_renderer_once() {
    diag_log("C8 install_renderer_once entered");
    if (!runtime_ready()) {
        diag_log("C8 runtime_ready FAIL");
        return false;
    }
    if (renderer_alive()) {
        diag_log("C8 renderer already alive");
        return true;
    }

    const auto now = platform_.monotonic_us();
    if (last_injected_pid_ == shellui_pid_ && last_injection_attempt_us_ != 0 &&
        now >= last_injection_attempt_us_ &&
        (now - last_injection_attempt_us_) < kSamePidInjectionCooldownUs) {
        diag_log("C8 injection cooldown");
        return false;
    }

    unsigned char* renderer = nullptr;
    std::size_t renderer_size = 0;
    if (!read_renderer_elf(&renderer, &renderer_size))
        return false;

    const pid_t target_pid = shellui_pid_;
    last_injected_pid_ = target_pid;
    last_injection_attempt_us_ = now;

    diag_log_i("C9 pt_attach target pid=", target_pid);
    if (pt_attach(target_pid) < 0) {
        diag_log("C9 pt_attach FAIL");
        std::free(renderer);
        return false;
    }
    diag_log("C9 pt_attach OK");

    intptr_t base_address = 0;
    diag_log("C10 elfldr_debug BEGIN");
    const int load_rc = elfldr_debug(-1, -1, -1, target_pid, renderer, &base_address);
    diag_log_i("C10 elfldr_debug rc=", load_rc);
    const int detach_rc = pt_detach(target_pid, 0);
    diag_log_i("C10 pt_detach rc=", detach_rc);
    std::free(renderer);
    if (load_rc < 0 || detach_rc < 0)
        return false;

    for (unsigned i = 0; i < 60; ++i) {
        platform_.sleep_ms(50);
        if (!refresh_shellui()) {
            diag_log("C11 ShellUI disappeared after inject");
            return false;
        }
        drain_health();
        if (heartbeat_fresh(last_receiver_heartbeat_us_)) {
            diag_log("C12 RC3 injection heartbeat PASS");
            return true;
        }
    }
    diag_log("C12 RC3 injection heartbeat TIMEOUT");
    return false;
}

} // namespace common_fps::ps5
