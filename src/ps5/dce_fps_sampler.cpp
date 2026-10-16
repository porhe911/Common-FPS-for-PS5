/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dce_fps_sampler.hpp"
#include "common_fps/fps_rate.hpp"
#include "common_fps/session_log.hpp"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <ps5/kernel.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace common_fps::ps5 {
namespace {

constexpr const char* kDceDevice = "/dev/dce";
constexpr const char* kSharedFpsSample = "/system_tmp/fps_sample";
constexpr std::uint32_t kSharedFpsMagic = 0x4f465053U; /* OFPS */
constexpr std::uint64_t kPtraceAuthId = 0x4800000000010003ULL;

constexpr unsigned long kDceFlipIoctl = 0x80308217UL;
constexpr unsigned long kDceFlipIoctlSignExtended =
    static_cast<unsigned long>(0xffffffff80308217ULL);

struct DceIoctlArg {
    std::uint64_t selector;
    std::uint64_t mask;
    std::uint64_t output;
    std::uint64_t reserved[3];
};

void log_line(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_message_v(kControllerLogPath, fmt, ap);
    va_end(ap);
}

bool begin_auth_window(
    std::uint64_t& original,
    bool& changed) noexcept {

    original = kernel_get_ucred_authid(getpid());
    changed = false;
    if (original == kPtraceAuthId)
        return true;

    if (kernel_set_ucred_authid(getpid(), kPtraceAuthId) != 0)
        return false;

    changed = true;
    return true;
}

bool end_auth_window(
    std::uint64_t original,
    bool changed) noexcept {

    return !changed ||
        kernel_set_ucred_authid(getpid(), original) == 0;
}

} // namespace

DceFpsSampler::DceFpsSampler(Platform& platform) noexcept
    : platform_(platform) {}

DceFpsSampler::~DceFpsSampler() {
    if (fd_ >= 0)
        close(fd_);
}

const char* DceFpsSampler::backend_name() const noexcept {
    return backend_name_;
}

std::optional<int> DceFpsSampler::sample_shared_hen() noexcept {
    const int fd = open(kSharedFpsSample, O_RDONLY);
    if (fd < 0)
        return std::nullopt;

    std::uint8_t buffer[128]{};
    const ssize_t count = read(fd, buffer, sizeof(buffer));
    close(fd);

    if (count < 20)
        return std::nullopt;

    std::uint32_t magic = 0;
    float fps = 0.0F;
    std::memcpy(&magic, buffer, sizeof(magic));
    std::memcpy(&fps, buffer + 16, sizeof(fps));

    const bool valid = buffer[12] != 0;
    if (magic != kSharedFpsMagic || !valid ||
        !validated_fps(static_cast<double>(fps))) {
        return std::nullopt;
    }

    const int rounded = *validated_fps(static_cast<double>(fps));
    if (rounded == 0)
        return std::nullopt;

    backend_name_ = "hen-shared";
    if (!backend_logged_) {
        backend_logged_ = true;
        log_line(
            "Fallback sampler online backend=hen-shared "
            "path=%s first_fps=%d",
            kSharedFpsSample,
            rounded);
    }
    return rounded;
}

bool DceFpsSampler::ensure_open() noexcept {
    if (fd_ >= 0)
        return true;

    errno = 0;
    fd_ = open(kDceDevice, O_RDWR);
    const int normal_errno = errno;
    if (fd_ >= 0) {
        consecutive_failures_ = 0;
        log_line(
            "DCE device online fd=%d open_mode=normal-rw",
            fd_);
        return true;
    }

    /*
     * Some firmwares expose /dev/dce only to privileged service credentials.
     * Use the same short auth-id window already used by the controller's
     * ptrace operations, then restore the original credentials immediately.
     */
    std::uint64_t original_auth = 0;
    bool changed = false;
    const bool auth_ready = begin_auth_window(original_auth, changed);
    int privileged_errno = 0;
    if (auth_ready) {
        errno = 0;
        fd_ = open(kDceDevice, O_RDWR);
        privileged_errno = errno;
        if (fd_ < 0) {
            errno = 0;
            fd_ = open(kDceDevice, O_RDONLY);
            if (privileged_errno == 0)
                privileged_errno = errno;
        }
    }
    const bool auth_restored =
        auth_ready && end_auth_window(original_auth, changed);

    if (fd_ < 0) {
        ++consecutive_failures_;
        if (consecutive_failures_ == 1 ||
            consecutive_failures_ % 15 == 0) {
            log_line(
                "DCE open failed normal_errno=%d privileged_errno=%d "
                "auth_ready=%d auth_changed=%d auth_restored=%d "
                "failures=%u",
                normal_errno,
                privileged_errno,
                auth_ready ? 1 : 0,
                changed ? 1 : 0,
                auth_restored ? 1 : 0,
                consecutive_failures_);
        }
        return false;
    }

    if (!auth_restored) {
        log_line(
            "DCE open succeeded but auth restore failed; closing fd=%d",
            fd_);
        close(fd_);
        fd_ = -1;
        return false;
    }

    consecutive_failures_ = 0;
    log_line(
        "DCE device online fd=%d open_mode=privileged-window "
        "normal_errno=%d auth_restored=1",
        fd_,
        normal_errno);
    return true;
}

bool DceFpsSampler::query_dce(
    std::array<std::uint64_t, 12>& words) noexcept {

    if (!ensure_open())
        return false;

    alignas(16) std::uint8_t output[0x60]{};
    DceIoctlArg arg{};
    arg.selector = 0x10000000AULL;
    arg.mask = 0x8000000000ULL;
    arg.output = reinterpret_cast<std::uint64_t>(output);

    errno = 0;
    int rc = ioctl(fd_, kDceFlipIoctl, &arg);
    int ioctl_errno = errno;
    unsigned long command = kDceFlipIoctl;

    if (rc < 0) {
        errno = 0;
        rc = ioctl(fd_, kDceFlipIoctlSignExtended, &arg);
        ioctl_errno = errno;
        command = kDceFlipIoctlSignExtended;
    }

    bool used_auth = false;
    bool auth_restored = true;
    int auth_errno = 0;

    if (rc < 0) {
        std::uint64_t original_auth = 0;
        bool changed = false;
        const bool auth_ready = begin_auth_window(original_auth, changed);
        if (auth_ready) {
            used_auth = true;
            std::memset(output, 0, sizeof(output));
            errno = 0;
            rc = ioctl(fd_, kDceFlipIoctl, &arg);
            auth_errno = errno;
            command = kDceFlipIoctl;
            if (rc < 0) {
                std::memset(output, 0, sizeof(output));
                errno = 0;
                rc = ioctl(fd_, kDceFlipIoctlSignExtended, &arg);
                auth_errno = errno;
                command = kDceFlipIoctlSignExtended;
            }
            auth_restored = end_auth_window(original_auth, changed);
        }
    }

    if (rc != 0 || !auth_restored) {
        ++consecutive_failures_;
        if (consecutive_failures_ == 1 ||
            consecutive_failures_ % 15 == 0) {
            log_line(
                "DCE ioctl failed rc=%d errno=%d auth_errno=%d "
                "auth_retry=%d auth_restored=%d command=0x%lx "
                "failures=%u",
                rc,
                ioctl_errno,
                auth_errno,
                used_auth ? 1 : 0,
                auth_restored ? 1 : 0,
                command,
                consecutive_failures_);
        }
        if (consecutive_failures_ >= 8) {
            close(fd_);
            fd_ = -1;
            reset();
        }
        return false;
    }

    consecutive_failures_ = 0;
    std::memcpy(words.data(), output, sizeof(output));

    static bool first_query_logged = false;
    if (!first_query_logged) {
        first_query_logged = true;
        log_line(
            "DCE query online command=0x%lx auth_retry=%d "
            "w0=%llu w1=%llu w2=%llu w3=%llu",
            command,
            used_auth ? 1 : 0,
            static_cast<unsigned long long>(words[0]),
            static_cast<unsigned long long>(words[1]),
            static_cast<unsigned long long>(words[2]),
            static_cast<unsigned long long>(words[3]));
    }

    return true;
}

void DceFpsSampler::reset() noexcept {
    counter_sampler_.reset();
    backend_logged_ = false;
    backend_name_ = "none";
}

std::optional<int> DceFpsSampler::sample() noexcept {
    if (const auto shared = sample_shared_hen())
        return shared;

    std::array<std::uint64_t, 12> words{};
    if (!query_dce(words))
        return std::nullopt;

    const int previous_word = counter_sampler_.selected_word();
    const auto fps = counter_sampler_.sample(words, platform_.monotonic_us());
    if (!fps)
        return std::nullopt;
    const int word = counter_sampler_.selected_word();
    backend_name_ = word == 1 ? "dce-fixed" : "dce-adaptive";
    if (word != 1 && word != previous_word) {
        log_line("DCE adaptive counter selected offset=0x%x fps=%d", word * 8, *fps);
    }
    if (!backend_logged_) {
        backend_logged_ = true;
        log_line("Fallback sampler online backend=%s offset=0x%x "
                 "first_fps=%d game_memory_reads=0 game_memory_writes=0",
                 backend_name_, word * 8, *fps);
    }
    return fps;
}

} // namespace common_fps::ps5
