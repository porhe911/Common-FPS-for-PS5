/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace common_fps {

inline constexpr const char* kControllerLogPath = "/data/CommonFPS.log";
inline constexpr const char* kRendererLogPath = "/data/CommonFPS_shellui.log";
inline constexpr std::size_t kSessionLogLimit = 64U * 1024U;

/* Close every descriptor immediately; preserve startup evidence when full. */
inline void write_session_log(
    const char* path, const char* data, std::size_t size,
    bool reset = false, bool durable = false) noexcept {
    const int fd = open(path,
        O_WRONLY | O_CREAT | O_APPEND | (durable ? O_SYNC : 0), 0644);
    if (fd < 0)
        return;
    int lock_rc;
    do {
        lock_rc = flock(fd, LOCK_EX);
    } while (lock_rc < 0 && errno == EINTR);
    if (lock_rc != 0) {
        close(fd);
        return;
    }

    struct stat status{};
    const bool ready = (!reset || ftruncate(fd, 0) == 0) &&
        fstat(fd, &status) == 0 && status.st_size >= 0;
    if (ready && static_cast<unsigned long long>(status.st_size) <
                     kSessionLogLimit) {
        size = std::min(size, kSessionLogLimit -
            static_cast<std::size_t>(status.st_size));
        while (size != 0) {
            const ssize_t written = write(fd, data, size);
            if (written > 0) {
                data += written;
                size -= static_cast<std::size_t>(written);
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
    }
    if (ready && durable)
        (void)fsync(fd);
    flock(fd, LOCK_UN);
    close(fd);
}

inline void reset_session_log(const char* path) noexcept {
    write_session_log(path, "", 0, true);
}

inline void log_message_v(
    const char* path, const char* format, va_list args,
    bool durable = false) noexcept {
    char line[2048]{};
    const int count = std::vsnprintf(line, sizeof(line) - 1, format, args);
    if (count < 0)
        return;
    const std::size_t size = std::min(
        static_cast<std::size_t>(count), sizeof(line) - 2);
    line[size] = '\n';
    write_session_log(path, line, size + 1, false, durable);
}

inline void log_message(const char* path, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    log_message_v(path, format, args);
    va_end(args);
}

/* Persist a checkpoint before entering the operation it identifies. */
inline void log_durable_message(
    const char* path, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    log_message_v(path, format, args, true);
    va_end(args);
}

} // namespace common_fps
