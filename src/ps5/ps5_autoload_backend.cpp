/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ps5_autoload_backend.hpp"

#include "common_fps/renderer_health.hpp"
#include "ps5_platform.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

extern "C" {
#include "elfldr.h"
#include "proc.h"
#include "pt.h"
}

namespace common_fps::ps5 {
namespace {

constexpr std::uint64_t kHeartbeatFreshUs = 2'500'000ULL;

/*
 * A failed/unfinished injection must never become a rapid reinjection loop.
 * The guard itself also backs off, but this second gate protects ShellUI even
 * if a caller changes the higher-level policy later.
 */
constexpr std::uint64_t kSamePidInjectionCooldownUs = 10'000'000ULL;

constexpr const char* kRendererPaths[] = {
    "/data/CommonFPS/Common_FPS_ShellUI_v1.1.0.elf",
    "/data/etaHEN/plugins/Common_FPS_ShellUI_v1.1.0.elf",
};

bool looks_like_elf(const std::vector<unsigned char>& data) {
    return
        data.size() >= 4 &&
        data[0] == 0x7f &&
        data[1] == 'E' &&
        data[2] == 'L' &&
        data[3] == 'F';
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
    if (health_socket_ >= 0)
        return true;

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

    if (bind(
            health_socket_,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0) {
        close(health_socket_);
        health_socket_ = -1;
        return false;
    }

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

    return pid > 0;
}

bool Ps5AutoloadBackend::mono_runtime_present(pid_t pid) {
    /*
     * The stable etaHEN ShellUI injection path relies on Mono.
     * Waiting for this native runtime module is a stronger signal than only
     * seeing the SceShellUI process name during early etaHEN startup.
     */
    module_info_t* mono = get_module_info(pid, "libmonosgen-2.0.sprx");
    if (!mono)
        return false;

    std::free(mono);
    return true;
}

bool Ps5AutoloadBackend::runtime_ready() {
    if (!valid())
        return false;

    if (!refresh_shellui())
        return false;

    return mono_runtime_present(shellui_pid_);
}

void Ps5AutoloadBackend::drain_health() {
    if (health_socket_ < 0)
        return;

    for (;;) {
        common_fps::RendererHealthPacket packet{};

        const ssize_t received = recv(
            health_socket_,
            &packet,
            sizeof(packet),
            0);

        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }

        if (static_cast<std::size_t>(received) != sizeof(packet))
            continue;

        if (!common_fps::valid_renderer_health_packet(packet))
            continue;

        if (packet.shellui_pid != shellui_pid_)
            continue;

        /*
         * Sequence is diagnostic/order protection only. Accept sequence zero
         * after a renderer restart because the ShellUI PID check already
         * separates different runtime instances.
         */
        if (last_health_sequence_ != 0 &&
            packet.sequence != 0 &&
            packet.sequence < last_health_sequence_) {
            continue;
        }

        last_health_sequence_ = packet.sequence;
        const std::uint64_t now = platform_.monotonic_us();
        last_receiver_heartbeat_us_ = now;

        if (packet.phase ==
            static_cast<std::uint16_t>(
                common_fps::RendererHealthPhase::VisualReady)) {
            last_visual_heartbeat_us_ = now;
        }
    }
}

bool Ps5AutoloadBackend::heartbeat_fresh(
    std::uint64_t timestamp_us) const {

    if (timestamp_us == 0)
        return false;

    const std::uint64_t now = platform_.monotonic_us();
    return now >= timestamp_us &&
           (now - timestamp_us) <= kHeartbeatFreshUs;
}

bool Ps5AutoloadBackend::visual_heartbeat_fresh(
    std::uint64_t timestamp_us) const {

    return heartbeat_fresh(timestamp_us);
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
    return visual_heartbeat_fresh(last_visual_heartbeat_us_);
}

bool Ps5AutoloadBackend::read_renderer_elf(
    unsigned char** data,
    std::size_t* size) const {

    if (!data || !size)
        return false;

    *data = nullptr;
    *size = 0;

    for (const char* path : kRendererPaths) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            continue;

        std::vector<unsigned char> bytes(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        /*
         * Reject an empty/corrupt/implausibly large file before ptrace.
         * The current companion is well below this ceiling.
         */
        if (!looks_like_elf(bytes) ||
            bytes.size() > 8U * 1024U * 1024U) {
            continue;
        }

        auto* copy = static_cast<unsigned char*>(
            std::malloc(bytes.size()));
        if (!copy)
            return false;

        std::memcpy(copy, bytes.data(), bytes.size());
        *data = copy;
        *size = bytes.size();
        return true;
    }

    return false;
}

bool Ps5AutoloadBackend::install_renderer_once() {
    if (!runtime_ready())
        return false;

    /*
     * If the companion is already running, never inject another copy.
     */
    if (renderer_alive())
        return true;

    const std::uint64_t now = platform_.monotonic_us();

    if (last_injected_pid_ == shellui_pid_ &&
        last_injection_attempt_us_ != 0 &&
        now >= last_injection_attempt_us_ &&
        (now - last_injection_attempt_us_) <
            kSamePidInjectionCooldownUs) {
        return false;
    }

    unsigned char* renderer = nullptr;
    std::size_t renderer_size = 0;

    if (!read_renderer_elf(&renderer, &renderer_size))
        return false;

    (void)renderer_size;

    const pid_t target_pid = shellui_pid_;
    last_injected_pid_ = target_pid;
    last_injection_attempt_us_ = now;

    /*
     * Preserve the complete v1.0.0 loader contract:
     *
     *   pt_attach()
     *       -> elfldr_debug()
     *            -> upstream elfldr_payload_args()
     *       -> pt_detach(pid, 0)
     *
     * elfldr_exec() is deliberately NOT used because its failure path can
     * SIGKILL the target process. Common FPS must never kill SceShellUI.
     */
    if (pt_attach(target_pid) < 0) {
        std::free(renderer);
        return false;
    }

    intptr_t base_address = 0;
    const int load_rc =
        elfldr_debug(-1, -1, -1, target_pid, renderer, &base_address);

    const int detach_rc = pt_detach(target_pid, 0);
    std::free(renderer);

    if (load_rc < 0 || detach_rc < 0)
        return false;

    /*
     * The target resumes only after pt_detach(). Give the injected entry point
     * a short bounded window to bind its receiver and emit a heartbeat.
     */
    for (unsigned i = 0; i < 60; ++i) {
        platform_.sleep_ms(50);

        if (!refresh_shellui())
            return false;

        drain_health();
        if (heartbeat_fresh(last_receiver_heartbeat_us_))
            return true;
    }

    return false;
}

} // namespace common_fps::ps5
