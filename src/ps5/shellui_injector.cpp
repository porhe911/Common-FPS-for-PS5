/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "shellui_injector.hpp"
#if defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
#elif defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY)
#include "shellui_auth_direct_probe.hpp"
#elif defined(COMMON_FPS_TEST27_AUTH_ONLY)
#include "shellui_auth_probe.hpp"
#elif defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY)
#include "shellui_attach_probe.hpp"
#else
#include "shellui_blob.hpp"
#include "stable_shellui_injector.hpp"
#endif

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

#if !defined(COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY)
extern "C" {
#include "proc.h"
}
#endif

namespace common_fps::ps5 {
namespace {

#if !defined(COMMON_FPS_TEST25_LOAD_ONLY) && \
    !defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY) && \
    !defined(COMMON_FPS_TEST27_AUTH_ONLY) && \
    !defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY) && \
    !defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
constexpr const char* kMarker = "/system_tmp/commonfps_shellui.pid";
#endif
#if defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test29_no_auth_only.log";
#elif defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test28_auth_direct_only.log";
#elif defined(COMMON_FPS_TEST27_AUTH_ONLY)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test27_auth_only.log";
#elif defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test26_attach_detach_only.log";
#elif defined(COMMON_FPS_TEST25_LOAD_ONLY)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test25_load_only_no_pthread.log";
#elif defined(COMMON_FPS_TEST31_TRACKED_RENDERER)
constexpr const char* kLog =
    "/data/CommonFPS_v110_test31_tracked_renderer.log";
#else
constexpr const char* kLog = "/data/CommonFPS_v110_source.log";
#endif
constexpr const char* kShellUiName = "SceShellUI";
pid_t g_last_shellui_pid = -1;

std::uint64_t now_ms() noexcept {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<std::uint64_t>(tv.tv_sec) * 1000ULL +
        static_cast<std::uint64_t>(tv.tv_usec) / 1000ULL;
}

/*
 * The FW 9.60 PARITY TEST2 binary proved this Sony kinfo_proc layout on
 * hardware.  PS5's returned record is not the stock FreeBSD structure from
 * the open SDK headers, so use checked byte offsets instead of casting it.
 */
constexpr long kSysctlSyscall = 202;
constexpr int kCtlKern = 1;
constexpr int kKernProc = 14;
constexpr int kKernProcProc = 8;
constexpr std::size_t kPidOffset = 72;
constexpr std::size_t kTdnameOffset = 447;

enum class ShellUiLookupPath {
    None,
    SysctlThreadName,
#if !defined(COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY)
    KernelAllprocFallback,
#endif
};

struct ShellUiLookupResult {
    pid_t pid = -1;
    ShellUiLookupPath path = ShellUiLookupPath::None;
    int size_query_rc = -1;
    int data_query_rc = -1;
    int saved_errno = 0;
    std::size_t bytes = 0;
    unsigned records = 0;
    unsigned malformed_records = 0;
};

bool exact_process_name(
    const char* field,
    std::size_t field_size) noexcept {

    constexpr std::size_t name_size = sizeof("SceShellUI") - 1;
    return field_size > name_size &&
        std::memcmp(field, kShellUiName, name_size) == 0 &&
        field[name_size] == '\0';
}

const char* lookup_path_name(ShellUiLookupPath path) noexcept {
    switch (path) {
    case ShellUiLookupPath::SysctlThreadName:
        return "sysctl_tdname";
#if !defined(COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY)
    case ShellUiLookupPath::KernelAllprocFallback:
        return "kernel_allproc";
#endif
    case ShellUiLookupPath::None:
        break;
    }
    return "none";
}

ShellUiLookupResult find_shellui_pid() noexcept {
    ShellUiLookupResult result{};

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

    if (result.size_query_rc == 0 && required != 0) {
        /* Leave headroom if the process list grows between the two calls. */
        const std::size_t capacity = required + required / 4 + 4096;
        auto* buffer = static_cast<std::uint8_t*>(std::malloc(capacity));

        if (buffer) {
            result.bytes = capacity;
            result.data_query_rc = static_cast<int>(syscall(
                kSysctlSyscall,
                mib,
                4,
                buffer,
                &result.bytes,
                nullptr,
                0));

            if (result.data_query_rc == 0) {
                std::uint8_t* cursor = buffer;
                const std::uint8_t* const end = buffer + result.bytes;

                while (cursor < end) {
                    const std::size_t remaining =
                        static_cast<std::size_t>(end - cursor);
                    if (remaining < sizeof(int)) {
                        ++result.malformed_records;
                        break;
                    }

                    int record_size = 0;
                    std::memcpy(
                        &record_size,
                        cursor,
                        sizeof(record_size));
                    if (record_size <= 0 ||
                        static_cast<std::size_t>(record_size) > remaining) {
                        ++result.malformed_records;
                        break;
                    }

                    ++result.records;
                    const std::size_t usable =
                        static_cast<std::size_t>(record_size);
                    const bool has_proven_fields = usable >=
                        kTdnameOffset + sizeof("SceShellUI");

                    pid_t candidate_pid = -1;
                    if (has_proven_fields) {
                        std::memcpy(
                            &candidate_pid,
                            cursor + kPidOffset,
                            sizeof(candidate_pid));
                    }

                    if (candidate_pid > 0 &&
                        candidate_pid != getpid() &&
                        exact_process_name(
                            reinterpret_cast<const char*>(
                                cursor + kTdnameOffset),
                            usable - kTdnameOffset)) {
                        result.pid = candidate_pid;
                        result.path =
                            ShellUiLookupPath::SysctlThreadName;
                        break;
                    }

                    cursor += record_size;
                }
            } else {
                result.saved_errno = errno;
            }

            std::free(buffer);
        } else {
            result.saved_errno = ENOMEM;
        }
    } else {
        result.saved_errno = errno;
    }

    if (result.pid > 0)
        return result;

#if !defined(COMMON_FPS_DIAGNOSTIC_SYSCTL_ONLY)
    /* Keep the former allproc path only as a secondary compatibility path. */
    if (struct proc* process = find_proc_by_name(kShellUiName)) {
        result.pid = process->pid;
        result.path = ShellUiLookupPath::KernelAllprocFallback;
        std::free(process);
    }
#endif

    return result;
}

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

#if !defined(COMMON_FPS_TEST25_LOAD_ONLY) && \
    !defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY) && \
    !defined(COMMON_FPS_TEST27_AUTH_ONLY) && \
    !defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY) && \
    !defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
bool marker_matches(pid_t pid) {
    FILE* fp = std::fopen(kMarker, "r");
    if (!fp)
        return false;

    int marked_pid = -1;
    const int rc = std::fscanf(fp, "%d", &marked_pid);
    std::fclose(fp);
    return rc == 1 && marked_pid == pid;
}
#endif

} // namespace

bool ensure_shellui_renderer() {
    static pid_t attempted_pid = -1;
    static bool attempted_result = false;
    static bool retry_allowed = true;
    static unsigned retry_delay_ticks = 0;
    static unsigned discovery_failures = 0;

    const ShellUiLookupResult lookup = find_shellui_pid();
    if (lookup.pid <= 0) {
        ++discovery_failures;
        if (discovery_failures == 1 || discovery_failures % 30 == 0) {
            log_line(
                "ShellUI lookup failed sysctl=%d/%d errno=%d "
                "bytes=%zu records=%u malformed=%u",
                lookup.size_query_rc,
                lookup.data_query_rc,
                lookup.saved_errno,
                lookup.bytes,
                lookup.records,
                lookup.malformed_records);
        }
        return false;
    }

    discovery_failures = 0;
    const pid_t pid = lookup.pid;
    g_last_shellui_pid = pid;

#if !defined(COMMON_FPS_TEST25_LOAD_ONLY) && \
    !defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY) && \
    !defined(COMMON_FPS_TEST27_AUTH_ONLY) && \
    !defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY) && \
    !defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
    if (marker_matches(pid))
        return true;
#endif

    if (attempted_pid != pid) {
        log_line(
            "ShellUI lookup pid=%d via=%s bytes=%zu records=%u "
            "malformed=%u",
            pid,
            lookup_path_name(lookup.path),
            lookup.bytes,
            lookup.records,
            lookup.malformed_records);
        attempted_pid = pid;
        attempted_result = false;
        retry_allowed = true;
        retry_delay_ticks = 0;
    } else if (attempted_result) {
        return true;
    } else if (!retry_allowed) {
        return false;
    } else if (retry_delay_ticks != 0) {
        --retry_delay_ticks;
        return false;
    }

    attempted_result = false;

#if defined(COMMON_FPS_TEST29_NO_AUTH_ONLY)
    log_line("ShellUI auth-free start pid=%d", pid);

    const std::uint64_t probe_started_ms = now_ms();
    attempted_result = true;
    retry_allowed = false;

    log_line(
        "ShellUI auth-free complete pid=%d auth_change=0 "
        "kernel_set_authid=0 ptrace_attach=0 ptrace_detach=0 sigcont=0 "
        "elf_load=0 payload_args=0 remote_mappings=0 stager=0 "
        "register_writes=0 remote_calls=0 elapsed_ms=%llu",
        pid,
        static_cast<unsigned long long>(now_ms() - probe_started_ms));
    return true;
#elif defined(COMMON_FPS_TEST28_AUTH_DIRECT_ONLY)
    log_line("ShellUI auth-direct start pid=%d", pid);

    const std::uint64_t probe_started_ms = now_ms();
    const ShellUiAuthDirectProbeResult probe =
        probe_shellui_direct_auth_transition();

    log_line(
        "ShellUI auth-direct ucred=%d original_read=%d change=%d "
        "target_verify=%d restore_verify=%d "
        "auth_before=0x%llx auth_during=0x%llx auth_after=0x%llx "
        "kernel_set_authid=0 ptrace_attach=0 ptrace_detach=0 sigcont=0 "
        "elf_load=0 payload_args=0 remote_mappings=0 stager=0 "
        "register_writes=0 remote_calls=0 elapsed_ms=%llu",
        probe.ucred_found ? 1 : 0,
        probe.original_read ? 1 : 0,
        probe.auth_changed ? 1 : 0,
        probe.direct_auth_verified ? 1 : 0,
        probe.auth_restored ? 1 : 0,
        static_cast<unsigned long long>(probe.original_auth),
        static_cast<unsigned long long>(probe.observed_direct_auth),
        static_cast<unsigned long long>(probe.observed_restored_auth),
        static_cast<unsigned long long>(now_ms() - probe_started_ms));

    const bool probe_complete =
        probe.ucred_found &&
        probe.original_read &&
        probe.auth_changed &&
        probe.direct_auth_verified &&
        probe.auth_restored;

    attempted_result = probe_complete;
    retry_allowed = false;

    if (!probe_complete) {
        log_line(
            "ShellUI auth-direct failed pid=%d retry=no",
            pid);
        return false;
    }

    log_line(
        "ShellUI auth-direct complete pid=%d total_ms=%llu",
        pid,
        static_cast<unsigned long long>(now_ms() - probe_started_ms));
    return true;
#elif defined(COMMON_FPS_TEST27_AUTH_ONLY)
    log_line("ShellUI ptrace-auth-only start pid=%d", pid);

    const std::uint64_t probe_started_ms = now_ms();
    const ShellUiAuthProbeResult probe =
        probe_shellui_ptrace_auth_transition();

    log_line(
        "ShellUI ptrace-auth-only original_read=%d change=%d "
        "target_verify=%d restore_verify=%d "
        "auth_before=0x%llx auth_during=0x%llx auth_after=0x%llx "
        "ptrace_attach=0 ptrace_detach=0 sigcont=0 "
        "elf_load=0 payload_args=0 remote_mappings=0 stager=0 "
        "register_writes=0 remote_calls=0 elapsed_ms=%llu",
        probe.original_read ? 1 : 0,
        probe.auth_changed ? 1 : 0,
        probe.ptrace_auth_verified ? 1 : 0,
        probe.auth_restored ? 1 : 0,
        static_cast<unsigned long long>(probe.original_auth),
        static_cast<unsigned long long>(probe.observed_ptrace_auth),
        static_cast<unsigned long long>(probe.observed_restored_auth),
        static_cast<unsigned long long>(now_ms() - probe_started_ms));

    const bool probe_complete =
        probe.original_read &&
        probe.auth_changed &&
        probe.ptrace_auth_verified &&
        probe.auth_restored;

    attempted_result = probe_complete;
    retry_allowed = false;

    if (!probe_complete) {
        log_line(
            "ShellUI ptrace-auth-only failed pid=%d retry=no",
            pid);
        return false;
    }

    log_line(
        "ShellUI ptrace-auth-only complete pid=%d total_ms=%llu",
        pid,
        static_cast<unsigned long long>(now_ms() - probe_started_ms));
    return true;
#elif defined(COMMON_FPS_TEST26_ATTACH_DETACH_ONLY)
    log_line("ShellUI attach-detach start pid=%d", pid);

    const std::uint64_t probe_started_ms = now_ms();
    const ShellUiAttachProbeResult probe =
        probe_shellui_attach_detach(pid);

    log_line(
        "ShellUI attach-detach auth=%d/%d attach=%d detach=%d "
        "sigcont=%d elf_load=0 payload_args=0 remote_mappings=0 "
        "stager=0 register_writes=0 remote_calls=0 "
        "elapsed_ms=%llu",
        probe.auth_changed ? 1 : 0,
        probe.auth_restored ? 1 : 0,
        probe.attached ? 1 : 0,
        probe.detached ? 1 : 0,
        probe.sigcont_sent ? 1 : 0,
        static_cast<unsigned long long>(now_ms() - probe_started_ms));

    const bool probe_complete =
        probe.auth_changed &&
        probe.auth_restored &&
        probe.attached &&
        probe.detached &&
        probe.sigcont_sent;

    if (!probe_complete) {
        retry_allowed = !probe.attached;
        retry_delay_ticks = retry_allowed ? 5U : 0U;
        log_line(
            "ShellUI attach-detach failed pid=%d retry=%s",
            pid,
            retry_allowed ? "yes" : "no");
        return false;
    }

    attempted_result = true;
    retry_allowed = false;
    log_line(
        "ShellUI attach-detach complete pid=%d total_ms=%llu",
        pid,
        static_cast<unsigned long long>(now_ms() - probe_started_ms));
    return true;
#else
    log_line(
        "ShellUI inject start pid=%d payload_size=%zu",
        pid,
        commonfps_shellui_elf_size);

    const std::uint64_t inject_started_ms = now_ms();

    const StableInjectionResult injected = inject_shellui_stable(
        pid,
        commonfps_shellui_elf,
        commonfps_shellui_elf_size,
#if defined(COMMON_FPS_TEST25_LOAD_ONLY)
        StableInjectionMode::ExerciseStagerWithoutThread);
#else
        StableInjectionMode::StartRendererThread);
#endif

#if defined(COMMON_FPS_TEST25_LOAD_ONLY)
    log_line(
        "ShellUI load-only auth=%d/%d attach=%d load=%d args=%d "
        "stager=%d functions=%d target_stack=%d call=%d "
        "thread_request=%d thread_created=%d rc=%d detach=%d "
        "trace=%d/%d imports=%u/%u first=%s elapsed_ms=%llu",
        injected.auth_changed ? 1 : 0,
        injected.auth_restored ? 1 : 0,
        injected.attached ? 1 : 0,
        injected.elf_loaded ? 1 : 0,
        injected.payload_args_ready ? 1 : 0,
        injected.remote_stager_ready ? 1 : 0,
        injected.remote_functions_ready ? 1 : 0,
        injected.target_stack_preserved ? 1 : 0,
        injected.bootstrap_started ? 1 : 0,
        injected.thread_start_requested ? 1 : 0,
        injected.pthread_create_ok ? 1 : 0,
        injected.pthread_create_rc,
        injected.detached ? 1 : 0,
        injected.trace_continue_seen ? 1 : 0,
        injected.trace_stop_seen ? 1 : 0,
        injected.imports_resolved,
        injected.imports_unresolved,
        injected.first_unresolved && *injected.first_unresolved
            ? injected.first_unresolved
            : "none",
        static_cast<unsigned long long>(now_ms() - inject_started_ms));

    const bool load_only_complete =
        injected.auth_changed &&
        injected.auth_restored &&
        injected.attached &&
        injected.elf_loaded &&
        injected.payload_args_ready &&
        injected.remote_stager_ready &&
        injected.remote_functions_ready &&
        injected.target_stack_preserved &&
        injected.bootstrap_started &&
        !injected.thread_start_requested &&
        !injected.pthread_create_ok &&
        injected.pthread_create_rc == kPthreadCreateSkippedRc &&
        injected.detached &&
        injected.trace_continue_seen &&
        injected.trace_stop_seen &&
        injected.imports_unresolved == 0;

    if (!load_only_complete) {
        retry_allowed = !injected.elf_loaded;
        retry_delay_ticks = retry_allowed ? 5U : 0U;
        log_line(
            "ShellUI load-only failed pid=%d retry=%s",
            pid,
            retry_allowed ? "yes" : "no");
        return false;
    }

    attempted_result = true;
    retry_allowed = false;
    log_line(
        "ShellUI image mapped no-thread pid=%d total_ms=%llu",
        pid,
        static_cast<unsigned long long>(now_ms() - inject_started_ms));
    return true;
#else
    log_line(
        "ShellUI bootstrap auth=%d/%d attach=%d load=%d args=%d "
        "target_stack=%d thread=%d rc=%d detach=%d trace=%d/%d "
        "imports=%u/%u first=%s elapsed_ms=%llu",
        injected.auth_changed ? 1 : 0,
        injected.auth_restored ? 1 : 0,
        injected.attached ? 1 : 0,
        injected.elf_loaded ? 1 : 0,
        injected.payload_args_ready ? 1 : 0,
        injected.target_stack_preserved ? 1 : 0,
        injected.bootstrap_started ? 1 : 0,
        injected.pthread_create_rc,
        injected.detached ? 1 : 0,
        injected.trace_continue_seen ? 1 : 0,
        injected.trace_stop_seen ? 1 : 0,
        injected.imports_resolved,
        injected.imports_unresolved,
        injected.first_unresolved && *injected.first_unresolved
            ? injected.first_unresolved
            : "none",
        static_cast<unsigned long long>(now_ms() - inject_started_ms));

    if (!injected.pthread_create_ok || !injected.auth_restored) {
        /*
         * A failure before the ELF image was committed is safe to retry after
         * a short delay.  Once a remote image/thread may exist, do not stack a
         * second copy in the same ShellUI process.
         */
        retry_allowed = !injected.elf_loaded;
        retry_delay_ticks = retry_allowed ? 5U : 0U;
        log_line(
            "ShellUI stable bootstrap failed pid=%d retry=%s",
            pid,
            retry_allowed ? "yes" : "no");
        return false;
    }

    /*
     * v1.0.0 waited for Scene/PUI readiness.  The source renderer retries for
     * up to 60 seconds, so the controller waits slightly longer for its
     * marker instead of starting game sampling prematurely.
     */
    for (int i = 0; i < 3500; ++i) {
        if (marker_matches(pid)) {
            attempted_result = true;
            log_line(
                "ShellUI renderer online pid=%d total_ms=%llu",
                pid,
                static_cast<unsigned long long>(
                    now_ms() - inject_started_ms));
            return true;
        }
        usleep(20000);
    }

    log_line("ShellUI marker timeout pid=%d", pid);
    retry_allowed = false;
    return false;
#endif
#endif
}

pid_t observe_shellui_pid() noexcept {
    const ShellUiLookupResult lookup = find_shellui_pid();
    g_last_shellui_pid = lookup.pid;
    return lookup.pid;
}

pid_t shellui_renderer_pid() noexcept {
    return g_last_shellui_pid;
}

} // namespace common_fps::ps5
