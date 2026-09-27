/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dce_fps_sampler.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace common_fps::ps5 {
namespace {

constexpr const char* kDceDevice = "/dev/dce";
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
        "/data/CommonFPS_universal_stage8_7.log", "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}

} // namespace

DceFpsSampler::DceFpsSampler(Platform& platform) noexcept
    : platform_(platform) {}

DceFpsSampler::~DceFpsSampler() {
    if (fd_ >= 0)
        close(fd_);
}

bool DceFpsSampler::ensure_open() noexcept {
    if (fd_ >= 0)
        return true;

    fd_ = open(kDceDevice, O_RDWR);
    if (fd_ < 0) {
        ++consecutive_failures_;
        if (consecutive_failures_ == 1 ||
            consecutive_failures_ % 30 == 0) {
            log_line(
                "DCE open failed path=%s failures=%u",
                kDceDevice,
                consecutive_failures_);
        }
        return false;
    }

    consecutive_failures_ = 0;
    log_line("DCE device online fd=%d", fd_);
    return true;
}

void DceFpsSampler::reset() noexcept {
    have_baseline_ = false;
    previous_flip_count_ = 0;
    previous_time_us_ = 0;
}

std::optional<int> DceFpsSampler::sample() noexcept {
    if (!ensure_open())
        return std::nullopt;

    alignas(16) std::uint8_t output[0x60]{};
    DceIoctlArg arg{};
    arg.selector = 0x10000000AULL;
    arg.mask = 0x8000000000ULL;
    arg.output = reinterpret_cast<std::uint64_t>(output);

    int rc = ioctl(fd_, kDceFlipIoctl, &arg);
    if (rc < 0)
        rc = ioctl(fd_, kDceFlipIoctlSignExtended, &arg);

    if (rc != 0) {
        ++consecutive_failures_;
        if (consecutive_failures_ == 1 ||
            consecutive_failures_ % 30 == 0) {
            log_line(
                "DCE ioctl failed rc=%d failures=%u",
                rc,
                consecutive_failures_);
        }
        if (consecutive_failures_ >= 10) {
            close(fd_);
            fd_ = -1;
            reset();
        }
        return std::nullopt;
    }

    consecutive_failures_ = 0;

    std::uint64_t flip_count = 0;
    std::memcpy(&flip_count, output + 8, sizeof(flip_count));
    const std::uint64_t now_us = platform_.monotonic_us();

    if (!have_baseline_) {
        previous_flip_count_ = flip_count;
        previous_time_us_ = now_us;
        have_baseline_ = true;
        return std::nullopt;
    }

    if (now_us <= previous_time_us_ ||
        flip_count < previous_flip_count_) {
        previous_flip_count_ = flip_count;
        previous_time_us_ = now_us;
        return std::nullopt;
    }

    const std::uint64_t elapsed_us = now_us - previous_time_us_;
    if (elapsed_us < 200000ULL)
        return std::nullopt;

    const std::uint64_t delta = flip_count - previous_flip_count_;
    previous_flip_count_ = flip_count;
    previous_time_us_ = now_us;

    const double fps =
        static_cast<double>(delta) * 1000000.0 /
        static_cast<double>(elapsed_us);

    if (fps < 1.0 || fps > 245.0)
        return std::nullopt;

    const int rounded = static_cast<int>(std::lround(fps));
    if (rounded < 1 || rounded > 245)
        return std::nullopt;

    if (!backend_logged_) {
        backend_logged_ = true;
        log_line(
            "DCE sampler online ioctl=0x%08lx first_fps=%d "
            "game_memory_reads=0 game_memory_writes=0",
            kDceFlipIoctl,
            rounded);
    }

    return rounded;
}

} // namespace common_fps::ps5
