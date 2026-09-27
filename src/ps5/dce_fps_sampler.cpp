/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dce_fps_sampler.hpp"

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
    FILE* fp = std::fopen(
        "/data/CommonFPS_universal_stage8_8.log", "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}

bool plausible_fps(double fps) noexcept {
    return fps >= 1.0 && fps <= 245.0;
}

int rounded_fps(double fps) noexcept {
    const int rounded = static_cast<int>(fps + 0.5);
    return rounded >= 1 && rounded <= 245 ? rounded : 0;
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
        fps < 1.0F || fps > 245.0F) {
        return std::nullopt;
    }

    const int rounded = rounded_fps(static_cast<double>(fps));
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

std::optional<int> DceFpsSampler::derive_fps(
    const std::array<std::uint64_t, 12>& words,
    std::uint64_t now_us) noexcept {

    if (!have_dce_baseline_) {
        previous_words_ = words;
        previous_time_us_ = now_us;
        have_dce_baseline_ = true;
        return std::nullopt;
    }

    if (now_us <= previous_time_us_)
        return std::nullopt;

    const std::uint64_t elapsed_us = now_us - previous_time_us_;
    if (elapsed_us < 200000ULL)
        return std::nullopt;

    auto fps_for_word = [&](std::size_t index) -> int {
        if (words[index] < previous_words_[index])
            return 0;
        const std::uint64_t delta =
            words[index] - previous_words_[index];
        const double fps =
            static_cast<double>(delta) * 1000000.0 /
            static_cast<double>(elapsed_us);
        return plausible_fps(fps) ? rounded_fps(fps) : 0;
    };

    /*
     * The known layout places the flip counter at +0x08. Keep that fast path
     * first. If firmware changed the returned structure, discover a stable
     * monotonic counter across the remaining aligned 64-bit fields.
     */
    int chosen_fps = 0;
    int chosen_word = selected_word_;

    const int fixed_fps = fps_for_word(1);
    if (fixed_fps != 0) {
        chosen_word = 1;
        chosen_fps = fixed_fps;
        selected_word_ = 1;
        selected_invalid_ = 0;
    } else if (selected_word_ >= 0 &&
               selected_word_ < static_cast<int>(words.size())) {
        chosen_fps = fps_for_word(
            static_cast<std::size_t>(selected_word_));
        if (chosen_fps == 0) {
            ++selected_invalid_;
            if (selected_invalid_ >= 3) {
                selected_word_ = -1;
                selected_invalid_ = 0;
            }
        } else {
            selected_invalid_ = 0;
        }
    }

    if (chosen_fps == 0 && selected_word_ < 0) {
        unsigned best_streak = 0;
        int best_word = -1;
        int best_fps = 0;

        for (std::size_t i = 0; i < words.size(); ++i) {
            const int fps = fps_for_word(i);
            if (fps == 0) {
                candidate_streak_[i] = 0;
                candidate_last_fps_[i] = 0;
                continue;
            }

            const int previous = candidate_last_fps_[i];
            const int difference =
                previous > fps ? previous - fps : fps - previous;

            if (previous != 0 && difference <= 12)
                ++candidate_streak_[i];
            else
                candidate_streak_[i] = 1;

            candidate_last_fps_[i] = fps;

            if (candidate_streak_[i] >= 2 &&
                candidate_streak_[i] > best_streak) {
                best_streak = candidate_streak_[i];
                best_word = static_cast<int>(i);
                best_fps = fps;
            }
        }

        if (best_word >= 0) {
            selected_word_ = best_word;
            chosen_word = best_word;
            chosen_fps = best_fps;
            log_line(
                "DCE adaptive counter selected offset=0x%x "
                "streak=%u fps=%d",
                best_word * 8,
                best_streak,
                best_fps);
        }
    }

    previous_words_ = words;
    previous_time_us_ = now_us;

    if (chosen_fps == 0)
        return std::nullopt;

    backend_name_ =
        chosen_word == 1 ? "dce-fixed" : "dce-adaptive";
    if (!backend_logged_) {
        backend_logged_ = true;
        log_line(
            "Fallback sampler online backend=%s offset=0x%x "
            "first_fps=%d game_memory_reads=0 game_memory_writes=0",
            backend_name_,
            chosen_word * 8,
            chosen_fps);
    }

    return chosen_fps;
}

void DceFpsSampler::reset() noexcept {
    have_dce_baseline_ = false;
    previous_words_.fill(0);
    previous_time_us_ = 0;
    candidate_streak_.fill(0);
    candidate_last_fps_.fill(0);
    selected_word_ = -1;
    selected_invalid_ = 0;
    backend_logged_ = false;
    backend_name_ = "none";
}

std::optional<int> DceFpsSampler::sample() noexcept {
    if (const auto shared = sample_shared_hen())
        return shared;

    std::array<std::uint64_t, 12> words{};
    if (!query_dce(words))
        return std::nullopt;

    return derive_fps(words, platform_.monotonic_us());
}

} // namespace common_fps::ps5
