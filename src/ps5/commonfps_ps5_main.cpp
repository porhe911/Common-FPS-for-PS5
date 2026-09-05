/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/fps_sampler.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/wire.hpp"
#include "ps5_platform.hpp"
#include "shellui_injector.hpp"
#include "state_sender.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

#if defined(COMMON_FPS_V111_SLEEP_RECOVERY)
constexpr const char* kControllerLog =
    "/data/CommonFPS_v111_sleep_recovery.log";
constexpr const char kRuntimeHeader[] =
    "Common FPS v1.1.1 sleep-recovery\n";
#else
constexpr const char* kControllerLog =
    "/data/CommonFPS_v110.log";
#endif

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

void write_all(int fd, const char* data, std::size_t size) noexcept {
    while (size != 0) {
        const ssize_t written = write(fd, data, size);
        if (written > 0) {
            data += written;
            size -= static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        break;
    }
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
    const ShellUiObservation& first) noexcept {

    const int fd = open(
        kControllerLog,
        O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
        0644);
    if (fd < 0)
        return;

    char record[1024]{};
#if defined(COMMON_FPS_V111_SLEEP_RECOVERY)
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "%s"
        "Mode=loader-tracked internal_fork=absent spawned_pid=resident "
        "shellui_observation=sysctl_tdname_1s "
        "renderer=shared_elf_etaHEN_update_hook "
        "injection=target_stack_pthread ipc=udp_loopback_1s "
        "mono_gc=pinned sampler=videoout_fw960_1s dmap=read_only "
        "shutdown_trace=disabled "
        "shutdown_writes=disabled signal_handlers=default "
        "stop_path=disabled\n"
        "Worker ready pid=%d ppid=%d first_shellui_pid=%d "
        "size_rc=%d data_rc=%d errno=%d bytes=%zu records=%u "
        "malformed=%u log_fd=closing periodic_log=disabled "
        "platform_log=compile_time_disabled\n",
        kRuntimeHeader,
        getpid(),
        getppid(),
        first.pid,
        first.size_query_rc,
        first.data_query_rc,
        first.saved_errno,
        first.bytes,
        first.records,
        first.malformed_records);
#else
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "Common FPS v1.1.0 tracked-process renderer\n"
        "Mode=loader-tracked internal_fork=absent spawned_pid=resident "
        "shellui_observation=sysctl_tdname_1s "
        "renderer=shared_elf_etaHEN_update_hook "
        "injection=target_stack_pthread ipc=udp_loopback_1s "
        "mono_gc=pinned sampler=videoout_fw960_1s dmap=read_only "
        "shutdown_trace=disabled "
        "shutdown_writes=disabled signal_handlers=default "
        "stop_path=disabled\n"
        "Worker ready pid=%d ppid=%d first_shellui_pid=%d "
        "size_rc=%d data_rc=%d errno=%d bytes=%zu records=%u "
        "malformed=%u log_fd=closing periodic_log=disabled "
        "platform_log=compile_time_disabled\n",
        getpid(),
        getppid(),
        first.pid,
        first.size_query_rc,
        first.data_query_rc,
        first.saved_errno,
        first.bytes,
        first.records,
        first.malformed_records);
#endif

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        write_all(fd, record, safe_size);
    }

    (void)fsync(fd);
    (void)close(fd);
}

#if defined(COMMON_FPS_V111_SLEEP_RECOVERY)
void append_shellui_lifecycle_record(
    pid_t previous_pid,
    pid_t current_pid) noexcept {

    const int fd = open(
        kControllerLog,
        O_WRONLY | O_CREAT | O_APPEND | O_SYNC,
        0644);
    if (fd < 0)
        return;

    const char* transition =
        previous_pid > 0 && current_pid <= 0
            ? "terminated"
            : previous_pid <= 0 && current_pid > 0
                ? "recreated"
                : "replaced";

    char record[256]{};
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "ShellUI lifecycle previous_pid=%d current_pid=%d "
        "transition=%s renderer_reset=1\n",
        previous_pid,
        current_pid,
        transition);

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        write_all(fd, record, safe_size);
    }

    (void)fsync(fd);
    (void)close(fd);
}
#endif

void append_first_fps_record(
    pid_t game_pid,
    std::uintptr_t counter_address,
    int fps) noexcept {

    const int fd = open(
        kControllerLog,
        O_WRONLY | O_CREAT | O_APPEND | O_SYNC,
        0644);
    if (fd < 0)
        return;

    char record[256]{};
    const int record_size = std::snprintf(
        record,
        sizeof(record),
        "Sampler online pid=%d counter=0x%llx first_fps=%d "
        "event_log=once_per_game_pid log_fd=closing\n",
        game_pid,
        static_cast<unsigned long long>(counter_address),
        fps);

    if (record_size > 0) {
        const std::size_t safe_size =
            static_cast<std::size_t>(record_size) < sizeof(record)
                ? static_cast<std::size_t>(record_size)
                : sizeof(record) - 1;
        write_all(fd, record, safe_size);
    }

    (void)fsync(fd);
    (void)close(fd);
}

[[noreturn]] void run_tracked_worker() noexcept {
    const ShellUiObservation first = observe_shellui_once();
    write_worker_ready_record(first);

    common_fps::ps5::Ps5Platform platform;
    common_fps::FpsSampler sampler(platform);
    common_fps::ps5::StateSender sender;
    pid_t reported_pid = -1;
#if defined(COMMON_FPS_V111_SLEEP_RECOVERY)
    pid_t observed_shellui_pid = first.pid;
#endif
    bool renderer_online = false;
    bool have_fps = false;
    int latest_fps = 0;
    std::uint64_t sequence = 1;
    const common_fps::OverlayConfig overlay_config{};

    /*
     * v1.1.0 keeps the tracked-process lifecycle correction: the sampler and
     * renderer controller run in the process already spawned and tracked by
     * etaHEN. There is no second internal fork, so etaHEN's PID file continues
     * to identify this worker. The only added path is the source-built ShellUI
     * renderer plus its one-way loopback state sender.
    */
    for (;;) {
#if defined(COMMON_FPS_V111_SLEEP_RECOVERY)
        const ShellUiObservation shellui = observe_shellui_once();
        if (shellui.pid != observed_shellui_pid) {
            append_shellui_lifecycle_record(
                observed_shellui_pid,
                shellui.pid);
            observed_shellui_pid = shellui.pid;
            renderer_online = false;
            have_fps = false;
        }

        /* A missing ShellUI is a renderer outage, not a permanent success. */
        if (shellui.pid <= 0)
            renderer_online = false;
#else
        (void)observe_shellui_once();
#endif

        if (!renderer_online)
            renderer_online = common_fps::ps5::ensure_shellui_renderer();

        if (!sampler.attached()) {
            have_fps = false;
            const auto game_pid = platform.find_game_process();
            if (game_pid)
                (void)sampler.attach(*game_pid);
        } else {
            const auto fps = sampler.sample();
            if (fps) {
                have_fps = true;
                latest_fps = *fps;
                if (sampler.pid() != reported_pid) {
                    reported_pid = sampler.pid();
                    append_first_fps_record(
                        reported_pid,
                        sampler.counter_address(),
                        *fps);
                }
            }
        }

        if (renderer_online) {
            common_fps::OverlayFrame frame{};
            frame.visible = true;
            frame.loading = !have_fps;
            frame.fps = latest_fps;
            frame.config = overlay_config;
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
