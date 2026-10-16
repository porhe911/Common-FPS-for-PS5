/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/fps_sampler.hpp"
#include "common_fps/fps_display_filter.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/wire.hpp"
#include "common_fps/session_log.hpp"
#include "common_fps/firmware_runtime_policy.hpp"
#include "stable_sampler/proc_rw_v960.hpp"
#include "ps5_platform.hpp"
#include "dce_fps_sampler.hpp"
#include "shellui_injector.hpp"
#include "state_sender.hpp"
#include "settings_service.hpp"
#include "media_launcher.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int sceKernelSetProcessName(const char* name);

namespace {

constexpr const char* kPayloadProcessName = "CommonFPS.elf";

constexpr std::uint64_t kDceVideoOutStandbyUs = 60'000'000ULL;
constexpr unsigned kDceMissesBeforeVideoOutWake = 3;

/*
 * These FW 9.60 kinfo_proc offsets were established by the hardware-proven
 * PARITY TEST2/TEST4 sysctl_tdname discovery path.
 */
constexpr long kSysctlSyscall = 202;
constexpr int kCtlKern = 1;
constexpr int kKernProc = 14;
constexpr int kKernProcProc = 8;
constexpr std::size_t kPidOffset = 72U;
constexpr std::size_t kTdnameOffset = 447U;
constexpr char kShellUiName[] = "SceShellUI";

struct ShellUiObservation {
    pid_t pid = -1;
    int size_query_rc = -1;
    int data_query_rc = -1;
    int saved_errno = 0;
    std::size_t bytes = 0;
    unsigned records = 0;
    unsigned malformed_records = 0;
};

struct PayloadIdentityResult {
    int process_name_rc = -1;
    int process_name_errno = 0;
    long thread_name_rc = -1;
    int thread_name_errno = 0;
};

PayloadIdentityResult set_payload_identity() noexcept {
    PayloadIdentityResult result{};

    errno = 0;
    result.process_name_rc = sceKernelSetProcessName(kPayloadProcessName);
    result.process_name_errno = result.process_name_rc == 0 ? 0 : errno;

    errno = 0;
    result.thread_name_rc = syscall(
        SYS_thr_set_name,
        -1,
        kPayloadProcessName);
    result.thread_name_errno = result.thread_name_rc == 0 ? 0 : errno;

    return result;
}

bool record_is_shellui(
    const std::uint8_t* record,
    std::size_t record_size) noexcept {

    if (!record || record_size < kTdnameOffset + sizeof(kShellUiName))
        return false;

    return std::memcmp(
        record + kTdnameOffset,
        kShellUiName,
        sizeof(kShellUiName)) == 0;
}

ShellUiObservation observe_shellui_once() noexcept {
    ShellUiObservation result{};
    int mib[4] = {
        kCtlKern,
        kKernProc,
        kKernProcProc,
        0,
    };

    std::size_t required = 0;
    result.size_query_rc = static_cast<int>(syscall(
        kSysctlSyscall,
        mib,
        4,
        nullptr,
        &required,
        nullptr,
        0));

    if (result.size_query_rc != 0 || required == 0) {
        result.saved_errno = errno;
        return result;
    }

    /* Allow the process list to grow between the two read-only queries. */
    const std::size_t capacity = required + required / 4U + 4096U;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));
    if (!buffer) {
        result.saved_errno = ENOMEM;
        return result;
    }

    result.bytes = capacity;
    result.data_query_rc = static_cast<int>(syscall(
        kSysctlSyscall,
        mib,
        4,
        buffer,
        &result.bytes,
        nullptr,
        0));

    if (result.data_query_rc != 0 || result.bytes > capacity) {
        result.saved_errno = result.bytes > capacity ? EOVERFLOW : errno;
        std::free(buffer);
        return result;
    }

    const pid_t self = getpid();
    const std::uint8_t* cursor = buffer;
    const std::uint8_t* const end = buffer + result.bytes;

    while (cursor < end) {
        const std::size_t remaining =
            static_cast<std::size_t>(end - cursor);
        if (remaining < sizeof(int)) {
            ++result.malformed_records;
            break;
        }

        int record_size_signed = 0;
        std::memcpy(
            &record_size_signed,
            cursor,
            sizeof(record_size_signed));

        if (record_size_signed <= 0 ||
            static_cast<std::size_t>(record_size_signed) > remaining) {
            ++result.malformed_records;
            break;
        }

        ++result.records;
        const std::size_t record_size =
            static_cast<std::size_t>(record_size_signed);

        if (record_size >= kPidOffset + sizeof(pid_t) &&
            record_is_shellui(cursor, record_size)) {
            pid_t candidate = -1;
            std::memcpy(
                &candidate,
                cursor + kPidOffset,
                sizeof(candidate));
            if (candidate > 0 && candidate != self) {
                result.pid = candidate;
                break;
            }
        }

        cursor += record_size;
    }

    std::free(buffer);
    return result;
}

void write_worker_ready_record(
    const ShellUiObservation& first,
    const PayloadIdentityResult& identity) noexcept {

    char record[2048]{};
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "Common FPS for PS5 v1.2.3\n"
        "sdk=0x%08x fps_limit=120 log_limit=65536 log_reset=activation "
        "fw1160_guard=ptrace_vm_protection_1byte\n"
        "ui_font=22 ui_bottom_margin=4 fps_display_filter=startup5s_median3_up3\n"
        "Mode=loader-tracked internal_fork=absent spawned_pid=resident "
        "shellui_observation=sysctl_tdname_1s stability_gate=10 "
        "game_gate=process_present_stable_3s "
        "renderer_injection=deferred_until_game "
        "renderer=shared_elf_stopped_chain_hook method_writes=controller_only "
        "injection=target_stack_pthread ipc=udp_loopback_1s "
        "mono_gc=pinned sampler=videoout_preferred+dce_auth_adaptive standby=60s_reprobe "
        "read=mdbg+dce_ioctl highfw_guard=firmware_routed_1byte "
        "shutdown_trace=disabled "
        "shutdown_writes=disabled signal_handlers=default "
        "stop_path=disabled\n"
        "Worker ready pid=%d ppid=%d first_shellui_pid=%d "
        "size_rc=%d data_rc=%d errno=%d bytes=%zu records=%u "
        "malformed=%u process_name=%s process_name_rc=%d "
        "process_name_errno=%d thread_name_rc=%ld thread_name_errno=%d "
        "log_fd=closing periodic_log=disabled platform_log=enabled\n",
        common_fps::ps5::stable_sampler::firmware_sdk_version(),
        getpid(),
        getppid(),
        first.pid,
        first.size_query_rc,
        first.data_query_rc,
        first.saved_errno,
        first.bytes,
        first.records,
        first.malformed_records,
        kPayloadProcessName,
        identity.process_name_rc,
        identity.process_name_errno,
        identity.thread_name_rc,
        identity.thread_name_errno);

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        common_fps::write_session_log(
            common_fps::kControllerLogPath, record, safe_size);
    }
}

void append_sampler_policy_record(
    pid_t game_pid,
    const char* state,
    const char* backend,
    std::uint64_t retry_after_us) noexcept {

    char record[320]{};
    const unsigned long long retry_ms =
        static_cast<unsigned long long>(retry_after_us / 1000ULL);
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "Sampler policy pid=%d state=%s backend=%s "
        "videoout_retry_after_ms=%llu\n",
        game_pid,
        state ? state : "unknown",
        backend ? backend : "unknown",
        retry_ms);

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        common_fps::write_session_log(
            common_fps::kControllerLogPath, record, safe_size);
    }
}

void append_first_fps_record(
    pid_t game_pid,
    std::uintptr_t counter_address,
    int fps,
    const char* backend) noexcept {

    char record[256]{};
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "Sampler online pid=%d backend=%s counter=0x%llx first_fps=%d "
        "event_log=once_per_backend_per_game_pid log_fd=closing\n",
        game_pid,
        backend ? backend : "unknown",
        static_cast<unsigned long long>(counter_address),
        fps);

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        common_fps::write_session_log(
            common_fps::kControllerLogPath, record, safe_size);
    }
}

[[noreturn]] void run_tracked_worker() noexcept {
    common_fps::reset_session_log(common_fps::kControllerLogPath);
    common_fps::reset_session_log(common_fps::kRendererLogPath);
    /*
     * Payload Manager's default Active Processes view recognizes resident
     * payloads by ki_comm ending in ".elf". Set both the process command name
     * and the main-thread name before the first diagnostic snapshot so the
     * controller appears as CommonFPS.elf instead of inheriting its loader
     * identity.
     */
    const PayloadIdentityResult identity = set_payload_identity();
    const ShellUiObservation first = observe_shellui_once();
    write_worker_ready_record(first, identity);

    if (common_fps::firmware_is_fw1160(
            common_fps::ps5::stable_sampler::firmware_sdk_version())) {
        common_fps::log_durable_message(common_fps::kControllerLogPath,
            "fw1160_runtime=experimental_full_counter sampler=enabled "
            "renderer=deferred_until_stable_game hardware_validation=rc9_user_confirmed "
            "fw1160_code_read=ptrace_then_checked_mdbg_on_efault");
    }

    common_fps::ps5::Ps5Platform platform;
    (void)mkdir("/data/CommonFPS", 0755);
    common_fps::SettingsStore settings(common_fps::kSettingsPath);
    if (!common_fps::ps5::ensure_media_launcher(
            common_fps::ps5::stable_sampler::firmware_sdk_version())) {
        common_fps::log_durable_message(common_fps::kControllerLogPath,
            "Media launcher credential restoration failed; sampler quarantined");
        for (;;) platform.sleep_ms(1000);
    }
    if (!common_fps::ps5::start_settings_service(settings))
        common_fps::log_durable_message(common_fps::kControllerLogPath,
            "Settings listener unavailable; saved overlay settings remain active");
    common_fps::FpsSampler sampler(platform);
    common_fps::ps5::DceFpsSampler dce_sampler(platform);
    common_fps::ps5::StateSender sender;
    common_fps::FpsDisplayFilter display_filter;
    pid_t reported_pid = -1;
    pid_t dce_reported_pid = -1;
    pid_t stable_shellui_pid = -1;
    unsigned stable_shellui_observations = 0;
    pid_t stable_game_pid = -1;
    unsigned stable_game_observations = 0;
    bool dce_standby_active = false;
    pid_t dce_standby_pid = -1;
    unsigned dce_miss_streak = 0;
    std::uint64_t next_videoout_probe_us = 0;
    bool renderer_online = false;
    bool have_fps = false;
    int latest_fps = 0;
    std::uint64_t sequence = 1;

    /*
     * The controller remains in etaHEN's tracked process: no second fork,
     * signal handler, shutdown write or stop path is introduced.  Stage 8
     * requires ten identical one-second ShellUI observations before the first
     * injection.  A changed PID after rest mode restarts the same gate.
     */
    for (;;) {
        const ShellUiObservation shellui = observe_shellui_once();
        if (shellui.pid > 0) {
            if (shellui.pid == stable_shellui_pid) {
                if (stable_shellui_observations < 10)
                    ++stable_shellui_observations;
            } else {
                stable_shellui_pid = shellui.pid;
                stable_shellui_observations = 1;
                renderer_online = false;
                display_filter.reset();
            }
        } else {
            stable_shellui_pid = -1;
            stable_shellui_observations = 0;
            renderer_online = false;
        }

        /*
         * Do not inject the ShellUI renderer on the home screen.  Earlier
         * hardware tests proved that touching Application.Update before a
         * game owns the "Game" container can make ShellUI restart.  The
         * controller remains loaded, but the renderer is started only after
         * the process sampler has observed the same real game process for
         * three consecutive one-second polls.  This also keeps autoload
         * completely inert between games and across reboot.
         */
        const auto observed_game_pid = platform.find_game_process();
        if (observed_game_pid) {
            if (*observed_game_pid == stable_game_pid) {
                if (stable_game_observations < 3)
                    ++stable_game_observations;
            } else {
                stable_game_pid = *observed_game_pid;
                stable_game_observations = 1;

                sampler.reset();
                reported_pid = -1;
                dce_sampler.reset();
                dce_reported_pid = -1;

                dce_standby_active = false;
                dce_standby_pid = -1;
                dce_miss_streak = 0;
                next_videoout_probe_us = 0;
            }
        } else {
            stable_game_pid = -1;
            stable_game_observations = 0;

            sampler.reset();
            reported_pid = -1;
            dce_sampler.reset();
            dce_reported_pid = -1;

            dce_standby_active = false;
            dce_standby_pid = -1;
            dce_miss_streak = 0;
            next_videoout_probe_us = 0;
        }
        const bool game_process_ready =
            stable_game_pid > 0 && stable_game_observations >= 3;

        const std::uint64_t now_us = platform.monotonic_us();
        const bool videoout_probe_due =
            !dce_standby_active ||
            now_us >= next_videoout_probe_us;

        std::optional<int> process_fps;
        if (sampler.attached()) {
            process_fps = sampler.sample();
        } else if (observed_game_pid && videoout_probe_due) {
            const bool attached = sampler.attach(*observed_game_pid);
            if (!attached && dce_standby_active) {
                next_videoout_probe_us =
                    now_us + kDceVideoOutStandbyUs;
            }
        }

        /*
         * v1.2.0 keeps VideoOut as the preferred source, but once the DCE
         * fallback has produced a valid FPS sample it suppresses expensive
         * read-only VideoOut discovery for 60 seconds.  A lightweight
         * periodic re-probe preserves the preferred path without rescanning
         * thousands of candidates every 10 seconds.
         */
        const auto dce_fps = game_process_ready
            ? dce_sampler.sample()
            : std::optional<int>{};

        auto display_source = common_fps::FpsDisplaySource::None;
        if (process_fps) {
            display_source = common_fps::FpsDisplaySource::VideoOut;
            have_fps = true;
            latest_fps = *process_fps;

            if (dce_standby_active) {
                append_sampler_policy_record(
                    sampler.pid(),
                    "videoout-resumed",
                    "videoout-process",
                    0);
            }
            dce_standby_active = false;
            dce_standby_pid = -1;
            dce_miss_streak = 0;
            next_videoout_probe_us = 0;

            if (sampler.pid() != reported_pid) {
                reported_pid = sampler.pid();
                append_first_fps_record(
                    reported_pid,
                    sampler.counter_address(),
                    *process_fps,
                    "videoout-process");
            }
        } else if (dce_fps && observed_game_pid) {
            const char* backend = dce_sampler.backend_name();
            display_source = std::strcmp(backend, "hen-shared") == 0
                ? common_fps::FpsDisplaySource::SharedHen
                : (std::strcmp(backend, "dce-fixed") == 0
                    ? common_fps::FpsDisplaySource::DceFixed
                    : common_fps::FpsDisplaySource::DceAdaptive);
            have_fps = true;
            latest_fps = *dce_fps;
            dce_miss_streak = 0;

            if (!sampler.attached()) {
                const bool entering_standby =
                    !dce_standby_active ||
                    dce_standby_pid != *observed_game_pid;

                dce_standby_active = true;
                dce_standby_pid = *observed_game_pid;

                if (entering_standby) {
                    next_videoout_probe_us =
                        now_us + kDceVideoOutStandbyUs;
                    append_sampler_policy_record(
                        *observed_game_pid,
                        "dce-standby",
                        dce_sampler.backend_name(),
                        kDceVideoOutStandbyUs);
                }
            }

            if (*observed_game_pid != dce_reported_pid) {
                dce_reported_pid = *observed_game_pid;
                append_first_fps_record(
                    dce_reported_pid,
                    0,
                    *dce_fps,
                    dce_sampler.backend_name());
            }
        } else {
            have_fps = false;

            if (dce_standby_active && observed_game_pid) {
                ++dce_miss_streak;
                if (dce_miss_streak >=
                    kDceMissesBeforeVideoOutWake) {
                    append_sampler_policy_record(
                        *observed_game_pid,
                        "dce-miss-wake",
                        dce_sampler.backend_name(),
                        0);
                    dce_standby_active = false;
                    dce_standby_pid = -1;
                    dce_miss_streak = 0;
                    next_videoout_probe_us = 0;
                }
            }
        }

        if (!renderer_online && game_process_ready &&
            stable_shellui_observations >= 10) {
            renderer_online =
                common_fps::ps5::ensure_shellui_renderer();
        }

        const auto display_fps = display_filter.sample(
            have_fps ? std::optional<int>{latest_fps} : std::nullopt,
            platform.monotonic_us(), stable_game_pid, display_source);
        if (renderer_online) {
            common_fps::OverlayFrame frame{};
            frame.visible = true;
            frame.loading = !display_fps;
            frame.fps = display_fps.value_or(0);
            frame.config = settings.get();
            frame.anchor = common_fps::compute_anchor(frame.config);
            (void)sender.send(common_fps::make_wire_packet(frame, sequence++));
        }

        platform.sleep_ms(1000);
    }
}

} // namespace

extern "C" int main() {
    run_tracked_worker();
}
