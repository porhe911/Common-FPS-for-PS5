/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "shellui_hook_controller.hpp"

#include "stable_sampler/proc_rw_v960.hpp"

#include <array>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
#include <ps5/mdbg.h>
#include "pt.h"
}

namespace common_fps::ps5 {
namespace {

constexpr std::uint64_t kPtraceAuthId = 0x4800000000010003ULL;
constexpr std::uint32_t kMinimumMdbgWriteSdk = 0x03000000U;
constexpr std::uint32_t kMaximumMdbgWriteSdk = 0x08200000U;

template <typename T>
bool read_exact_file(const char* path, T& value) noexcept {
    FILE* fp = std::fopen(path, "rb");
    if (!fp)
        return false;

    const std::size_t count = std::fread(&value, 1, sizeof(value), fp);
    const int trailing = std::fgetc(fp);
    std::fclose(fp);
    return count == sizeof(value) && trailing == EOF;
}

template <typename T>
bool write_exact_file_atomic(
    const char* temporary_path,
    const char* final_path,
    const T& value) noexcept {

    FILE* fp = std::fopen(temporary_path, "wb");
    if (!fp)
        return false;

    const bool complete =
        std::fwrite(&value, 1, sizeof(value), fp) == sizeof(value) &&
        std::fflush(fp) == 0;
    const bool closed = std::fclose(fp) == 0;
    if (!complete || !closed) {
        (void)unlink(temporary_path);
        return false;
    }

    if (std::rename(temporary_path, final_path) != 0) {
        (void)unlink(temporary_path);
        return false;
    }
    return true;
}

bool request_is_valid(
    const ShellUiHookRequest& request,
    pid_t shellui_pid) noexcept {

    return request.magic == kShellUiHookRequestMagic &&
        request.version == kShellUiHookProtocolVersion &&
        request.pid == shellui_pid &&
        request.nonce != 0 &&
        request.method_address >= 0x10000ULL &&
        request.hook_address >= 0x10000ULL &&
        request.trampoline_address >= 0x10000ULL &&
        request.patch_size == kShellUiHookPatchSize &&
        request.displaced_size >= 14 &&
        request.displaced_size <= kShellUiHookPatchSize &&
        request.checksum == shellui_hook_request_checksum(request);
}

void write_ack(
    const ShellUiHookRequest& request,
    const ShellUiHookPatchReport& report) noexcept {

    ShellUiHookAck ack{};
    ack.pid = request.pid;
    ack.nonce = request.nonce;
    ack.status = static_cast<std::int32_t>(report.status);
    ack.read_rc = report.read_rc;
    ack.write_rc = report.write_rc;
    ack.verified = report.verified ? 1U : 0U;
    ack.restored = report.restored ? 1U : 0U;
    ack.detached = report.detached ? 1U : 0U;
    ack.auth_restored = report.auth_restored ? 1U : 0U;
    ack.checksum = shellui_hook_ack_checksum(ack);
    (void)write_exact_file_atomic(
        kShellUiHookAckTempPath,
        kShellUiHookAckPath,
        ack);
}

} // namespace

void clear_shellui_hook_protocol_files() noexcept {
    (void)unlink(kShellUiHookRequestPath);
    (void)unlink(kShellUiHookRequestTempPath);
    (void)unlink(kShellUiHookAckPath);
    (void)unlink(kShellUiHookAckTempPath);
}

ShellUiHookPollResult poll_and_apply_shellui_hook(
    pid_t shellui_pid,
    ShellUiHookPatchReport& report) noexcept {

    report = {};
    ShellUiHookRequest request{};
    if (!read_exact_file(kShellUiHookRequestPath, request))
        return ShellUiHookPollResult::NoRequest;

    /* Consume exactly one atomically-published request. */
    (void)unlink(kShellUiHookRequestPath);

    report.sdk_version = stable_sampler::firmware_sdk_version();
    report.method_address = request.method_address;
    report.displaced_size = request.displaced_size;

    if (!request_is_valid(request, shellui_pid)) {
        report.status = ShellUiHookStatus::InvalidRequest;
        write_ack(request, report);
        return ShellUiHookPollResult::Failed;
    }

    const std::uint32_t sdk_family =
        report.sdk_version & 0xffff0000U;
    if (sdk_family < kMinimumMdbgWriteSdk ||
        sdk_family > kMaximumMdbgWriteSdk) {
        report.status = ShellUiHookStatus::UnsupportedFirmware;
        write_ack(request, report);
        return ShellUiHookPollResult::Failed;
    }

    const pid_t self = getpid();
    const std::uint64_t original_auth = kernel_get_ucred_authid(self);
    if (original_auth == 0 ||
        kernel_set_ucred_authid(self, kPtraceAuthId) != 0) {
        report.status = ShellUiHookStatus::AuthFailure;
        write_ack(request, report);
        return ShellUiHookPollResult::Failed;
    }

    bool attached = false;
    bool patch_applied = false;
    std::array<std::uint8_t, kShellUiHookPatchSize> observed{};
    std::array<std::uint8_t, kShellUiHookPatchSize> verified{};

    if (pt_attach(shellui_pid) != 0) {
        report.status = ShellUiHookStatus::AttachFailure;
    } else {
        attached = true;
        report.read_rc = mdbg_copyout(
            shellui_pid,
            static_cast<intptr_t>(request.method_address),
            observed.data(),
            observed.size());

        if (report.read_rc != 0 ||
            std::memcmp(
                observed.data(),
                request.expected,
                observed.size()) != 0) {
            report.status = ShellUiHookStatus::ExpectedBytesMismatch;
        } else {
            report.expected_matched = true;
            report.write_rc = mdbg_copyin(
                shellui_pid,
                request.desired,
                static_cast<intptr_t>(request.method_address),
                kShellUiHookPatchSize);

            const int verify_rc = mdbg_copyout(
                shellui_pid,
                static_cast<intptr_t>(request.method_address),
                verified.data(),
                verified.size());
            report.verified = verify_rc == 0 &&
                std::memcmp(
                    verified.data(),
                    request.desired,
                    verified.size()) == 0;
            if (report.verified) {
                patch_applied = true;
                report.status = ShellUiHookStatus::Success;
            } else {
                report.status = report.write_rc == 0
                    ? ShellUiHookStatus::VerifyFailure
                    : ShellUiHookStatus::WriteFailure;

                const bool still_original = verify_rc == 0 &&
                    std::memcmp(
                        verified.data(),
                        request.expected,
                        verified.size()) == 0;
                if (still_original) {
                    report.restored = true;
                } else {
                    const int restore_rc = mdbg_copyin(
                        shellui_pid,
                        request.expected,
                        static_cast<intptr_t>(request.method_address),
                        kShellUiHookPatchSize);
                    std::array<std::uint8_t, kShellUiHookPatchSize>
                        restore_check{};
                    report.restored = restore_rc == 0 &&
                        mdbg_copyout(
                            shellui_pid,
                            static_cast<intptr_t>(request.method_address),
                            restore_check.data(),
                            restore_check.size()) == 0 &&
                        std::memcmp(
                            restore_check.data(),
                            request.expected,
                            restore_check.size()) == 0;
                }
            }
        }
    }

    if (attached) {
        report.detached = pt_detach(shellui_pid, 0) == 0;
        (void)kill(shellui_pid, SIGCONT);
        if (!report.detached && patch_applied)
            report.status = ShellUiHookStatus::DetachFailure;
    }

    if (kernel_set_ucred_authid(self, original_auth) == 0) {
        report.auth_restored =
            kernel_get_ucred_authid(self) == original_auth;
    }
    if (!report.auth_restored && patch_applied)
        report.status = ShellUiHookStatus::AuthRestoreFailure;

    write_ack(request, report);
    return report.status == ShellUiHookStatus::Success
        ? ShellUiHookPollResult::Applied
        : ShellUiHookPollResult::Failed;
}

} // namespace common_fps::ps5
