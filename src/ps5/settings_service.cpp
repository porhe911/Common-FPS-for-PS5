/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_service.hpp"
#include "common_fps/session_log.hpp"
#include "common_fps/config.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

extern "C" {
extern const unsigned char commonfps_settings_html[];
extern const std::size_t commonfps_settings_html_size;
}

namespace common_fps::ps5 {
namespace {
struct Worker { int listener; SettingsStore* store; };

std::uint64_t monotonic_ms() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<std::uint64_t>(t.tv_sec)*1000 + t.tv_nsec/1'000'000;
}

void client(int fd, SettingsStore& store) {
    const timeval timeout{1, 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
    const int enabled = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    const auto deadline = monotonic_ms() + 2000;
    char buffer[kMaxSettingsBytes+1]{};
    std::size_t used = 0;
    while (used < kMaxSettingsBytes && monotonic_ms() < deadline) {
        const auto n = recv(fd, buffer+used, kMaxSettingsBytes-used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        used += static_cast<std::size_t>(n);
        buffer[used] = 0;
        const char* end = std::strstr(buffer, "\r\n\r\n");
        if (!end) continue;
        // The full parser remains authoritative, including duplicate headers.
        std::string headers(buffer, end-buffer);
        for (char& c : headers) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
        const auto key = headers.find("\r\ncontent-length:");
        const unsigned long length = key == std::string::npos ? 0 :
            std::strtoul(headers.c_str()+key+17, nullptr, 10);
        if (length > 512 || used >= static_cast<std::size_t>(end-buffer)+4+length) break;
    }
    const auto reply = settings_http_response(std::string_view(buffer, used), store,
        std::string_view(reinterpret_cast<const char*>(commonfps_settings_html), commonfps_settings_html_size));
    if (std::string_view(buffer, used).find("POST /api/config ") == 0 &&
        reply.find("HTTP/1.1 200 ") == 0) {
        const auto cfg = store.get();
        log_durable_message(kControllerLogPath,
            "Settings saved corner=%s font=%d margins=%.0f/%.0f label=%d colors=%06x/%06x",
            corner_name(cfg.corner), cfg.font_size, cfg.margin_x, cfg.margin_y,
            cfg.show_label ? 1 : 0, cfg.label_color, cfg.value_color);
    }
    std::size_t sent = 0;
    const auto send_deadline = monotonic_ms()+2000;
    while (sent < reply.size() && monotonic_ms() < send_deadline) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto n = send(fd, reply.data()+sent, reply.size()-sent, flags);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

void* run(void* arg) {
    Worker worker = *static_cast<Worker*>(arg);
    delete static_cast<Worker*>(arg);
    for (;;) {
        const int fd = accept(worker.listener, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR) continue;
            usleep(100'000);
            continue;
        }
        client(fd, *worker.store);
        close(fd);
    }
    return nullptr;
}
} // namespace

bool start_settings_service(SettingsStore& store) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    const int reuse = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kSettingsPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(fd, 4) != 0) {
        close(fd);
        return false;
    }
    auto* worker = new Worker{fd, &store};
    pthread_t thread{};
    const int rc = pthread_create(&thread, nullptr, run, worker);
    if (rc != 0) { delete worker; close(fd); return false; }
    (void)pthread_detach(thread);
    log_durable_message(kControllerLogPath,
        "Settings online http://127.0.0.1:39029/ config=/data/CommonFPS/config.ini settings=media_live_v1");
    return true;
}
} // namespace common_fps::ps5
