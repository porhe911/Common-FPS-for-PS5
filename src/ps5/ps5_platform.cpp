/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * PS5 adapter for Common FPS.
 *
 * FW 9.60 adapter for the hardware-proven Common FPS sampler:
 *   KERN_PROC PID discovery -> short debugger Auth window for module lookup
 *   -> original Auth restore -> read-only DMAP process reads.
 *
 * This file is GPL-3.0-or-later and is intended to be built against
 * the etaHEN Plugin SDK / PS5 Payload SDK.
 */

#include "ps5_platform.hpp"

#include "common_fps/constants.hpp"
#include "stable_sampler/process_sysctl.hpp"
#include "stable_sampler/proc_rw_v960.hpp"
#include "stable_sampler/videoout_module.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

namespace common_fps::ps5 {
namespace {

constexpr const char* kVideoOutModule = "libSceVideoOut.sprx";
#if !defined(COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG)
constexpr std::size_t kProbeTableSize =
    kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;
#endif

#if defined(COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG)

/* Diagnostic A/B builds observe the sampler without recurring file writes. */
#define log_line(...) ((void)0)

#else

constexpr const char* kLog = "/data/CommonFPS_v110_source.log";

void log_line(const char* fmt, ...) {
    FILE* fp = std::fopen(kLog, "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}

unsigned long long hex_address(std::uintptr_t value) noexcept {
    return static_cast<unsigned long long>(value);
}

#endif

} // namespace

std::optional<ProcessId> Ps5Platform::find_game_process() {
    const auto lookup = stable_sampler::find_game_process_sysctl();

    if (lookup.pid > 0) {
        empty_lookup_count_ = 0;
        if (observed_game_pid_ != lookup.pid) {
            observed_game_pid_ = lookup.pid;
            module_attempt_pid_ = -1;
            module_attempt_count_ = 0;
            videoout_base_ = 0;
            table_read_logged_ = false;
            root_read_logged_ = false;
            counter_read_logged_ = false;
            read_failure_count_ = 0;

            log_line(
                "Sampler game pid=%d via=sysctl bytes=%zu records=%u "
                "malformed=%u",
                lookup.pid,
                lookup.bytes,
                lookup.records,
                lookup.malformed_records);
        }
        return lookup.pid;
    }

    ++empty_lookup_count_;
    if (empty_lookup_count_ == 1 || empty_lookup_count_ % 30 == 0) {
        log_line(
            "Sampler game lookup empty sysctl=%d/%d errno=%d "
            "bytes=%zu records=%u malformed=%u",
            lookup.size_query_rc,
            lookup.data_query_rc,
            lookup.saved_errno,
            lookup.bytes,
            lookup.records,
            lookup.malformed_records);
    }

    observed_game_pid_ = -1;
    return std::nullopt;
}

bool Ps5Platform::process_alive(ProcessId pid) {
    const auto lookup = stable_sampler::find_game_process_sysctl();
    if (lookup.pid == pid)
        return true;

    log_line(
        "Sampler game changed expected=%d current=%d sysctl=%d/%d "
        "errno=%d",
        pid,
        lookup.pid,
        lookup.size_query_rc,
        lookup.data_query_rc,
        lookup.saved_errno);

    observed_game_pid_ = lookup.pid;
    module_attempt_pid_ = -1;
    videoout_base_ = 0;
    table_read_logged_ = false;
    root_read_logged_ = false;
    counter_read_logged_ = false;
    return false;
}

std::optional<ModuleInfo>
Ps5Platform::find_module(ProcessId pid, const char* module_name) {
    if (!module_name || std::strcmp(module_name, kVideoOutModule) != 0)
        return std::nullopt;

    if (module_attempt_pid_ != pid) {
        module_attempt_pid_ = pid;
        module_attempt_count_ = 0;
    }

    ++module_attempt_count_;
    const auto result = stable_sampler::find_videoout_module(pid);
    const bool success = result.base != 0 && result.auth_restored;
    const bool first_success_for_base =
        success && videoout_base_ != result.base;

    if (first_success_for_base || module_attempt_count_ == 1 ||
        module_attempt_count_ % 10 == 0) {
        log_line(
            "Sampler module pid=%d auth=%d/%d/%d/%d "
            "list=%ld/%ld count=%zu/%zu info=%u last=%ld "
            "base=0x%llx result=%s",
            pid,
            result.auth_read ? 1 : 0,
            result.auth_changed ? 1 : 0,
            result.auth_restore_attempted ? 1 : 0,
            result.auth_restored ? 1 : 0,
            result.size_query_rc,
            result.fill_query_rc,
            result.requested_count,
            result.returned_count,
            result.info_successes,
            result.last_info_rc,
            hex_address(result.base),
            success ? "ok" : "failed");
    }

    if (!success)
        return std::nullopt;

    if (videoout_base_ != result.base) {
        videoout_base_ = result.base;
        table_read_logged_ = false;
        root_read_logged_ = false;
        counter_read_logged_ = false;
        read_failure_count_ = 0;
    }

    return ModuleInfo{result.base, kVideoOutModule};
}

bool Ps5Platform::read_memory(
    ProcessId pid,
    std::uintptr_t address,
    void* out,
    std::size_t size) {

    const bool read_ok = stable_sampler::proc_read(
        pid,
        address,
        out,
        size);

    if (!read_ok) {
        ++read_failure_count_;
        if (read_failure_count_ == 1 || read_failure_count_ % 10 == 0) {
            log_line(
                "Sampler DMAP read failed pid=%d address=0x%llx "
                "size=%zu sdk=0x%08x dmap=0x%llx failures=%u",
                pid,
                hex_address(address),
                size,
                stable_sampler::firmware_sdk_version(),
                hex_address(stable_sampler::dmap_base()),
                read_failure_count_);
        }
        return false;
    }

    read_failure_count_ = 0;

#if !defined(COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG)
    if (size == kProbeTableSize &&
        address == videoout_base_ + kVideoOutProbeTableOffset &&
        !table_read_logged_) {
        unsigned enabled_records = 0;
        std::uint64_t first_pointer = 0;
        const auto* table = static_cast<const std::uint8_t*>(out);
        for (std::size_t i = 0; i < kVideoOutProbeEntryCount; ++i) {
            const auto* entry = table + i * kVideoOutProbeEntrySize;
            std::uint32_t enabled = 0;
            std::uint64_t pointer = 0;
            std::memcpy(&enabled, entry, sizeof(enabled));
            std::memcpy(&pointer, entry + 0x08, sizeof(pointer));
            if (enabled != 0 && pointer != 0) {
                ++enabled_records;
                if (first_pointer == 0)
                    first_pointer = pointer;
            }
        }

        table_read_logged_ = true;
        log_line(
            "Sampler table read pid=%d address=0x%llx size=%zu "
            "sdk=0x%08x dmap=0x%llx enabled=%u first=0x%llx",
            pid,
            hex_address(address),
            size,
            stable_sampler::firmware_sdk_version(),
            hex_address(stable_sampler::dmap_base()),
            enabled_records,
            static_cast<unsigned long long>(first_pointer));
    } else if (size == sizeof(std::uint64_t) && !root_read_logged_) {
        std::uint64_t root = 0;
        std::memcpy(&root, out, sizeof(root));
        root_read_logged_ = root != 0;
        log_line(
            "Sampler probe root pid=%d pointer=0x%llx root=0x%llx",
            pid,
            hex_address(address),
            static_cast<unsigned long long>(root));
    } else if (size == sizeof(std::uint32_t) && !counter_read_logged_) {
        std::uint32_t counter = 0;
        std::memcpy(&counter, out, sizeof(counter));
        counter_read_logged_ = true;
        log_line(
            "Sampler counter online pid=%d address=0x%llx value=%u",
            pid,
            hex_address(address),
            counter);
    }
#endif

    return true;
}

std::uint64_t Ps5Platform::monotonic_us() {
    timeval tv{};
    gettimeofday(&tv, nullptr);

    return
        static_cast<std::uint64_t>(tv.tv_sec) * 1'000'000ULL +
        static_cast<std::uint64_t>(tv.tv_usec);
}

void Ps5Platform::sleep_ms(unsigned milliseconds) {
    usleep(static_cast<useconds_t>(milliseconds) * 1000U);
}

#if defined(COMMON_FPS_DIAGNOSTIC_NO_PLATFORM_LOG)
#undef log_line
#endif

} // namespace common_fps::ps5
