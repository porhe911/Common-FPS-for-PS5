/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace common_fps {

inline constexpr const char* kShellUiHookRequestPath =
    "/system_tmp/commonfps_v122_hook_request.bin";
inline constexpr const char* kShellUiHookRequestTempPath =
    "/system_tmp/commonfps_v122_hook_request.tmp";
inline constexpr const char* kShellUiHookAckPath =
    "/system_tmp/commonfps_v122_hook_ack.bin";
inline constexpr const char* kShellUiHookAckTempPath =
    "/system_tmp/commonfps_v122_hook_ack.tmp";

inline constexpr std::uint64_t kShellUiHookRequestMagic =
    0x395145524b484643ULL; /* "CFHKREQ9" */
inline constexpr std::uint64_t kShellUiHookAckMagic =
    0x394b43414b484643ULL; /* "CFHKACK9" */
inline constexpr std::uint32_t kShellUiHookProtocolVersion = 9;
inline constexpr std::size_t kShellUiHookPatchSize = 16;
inline constexpr std::uint8_t kShellUiAbsoluteJumpPrefix[6] = {
    0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
};

enum class ShellUiHookBackend { Unsupported, Mdbg, PtraceIo };

enum class ShellUiHookAckPhase : std::uint8_t {
    Patch = 0,
    Probe = 1,
    MainThreadGuard = 2,
};

inline ShellUiHookBackend shellui_hook_backend(
    std::uint32_t sdk, bool eta_chain) noexcept {
    const std::uint32_t family = sdk & 0xffff0000U;
    if (family >= 0x03000000U && family <= 0x08200000U)
        return ShellUiHookBackend::Mdbg;
    if (family == 0x09600000U && eta_chain)
        return ShellUiHookBackend::PtraceIo;
    return ShellUiHookBackend::Unsupported;
}

/*
 * The background-PUI path never replaces Application.Update. It changes only
 * the first byte of Diagnostics.CheckRunningOnMainThread to RET while ShellUI
 * is ptrace-stopped. FW 1.xx-8.20 uses MDBG; selected higher firmware uses
 * ptrace I/O. FW 9.60 deliberately keeps the hardware-proven update-chain
 * backend. Unknown future firmware fails closed.
 */
inline ShellUiHookBackend shellui_main_thread_guard_backend(
    std::uint32_t sdk) noexcept {
    const std::uint32_t family = sdk & 0xffff0000U;

    /*
     * Low/mid firmware uses the controller MDBG backend.  Higher firmware
     * keeps the same one-byte, expected-byte-verified guard transaction but
     * uses ptrace I/O, which is already required by the ShellUI loader.
     *
     * FW 9.60 deliberately stays on the hardware-proven Application.Update
     * chain path; returning Unsupported here selects that existing backend.
     */
    if ((family >= 0x01000000U && family <= 0x08200000U) ||
        family == 0x09000000U)
        return ShellUiHookBackend::Mdbg;
    if ((family >= 0x08300000U && family < 0x09000000U) ||
        (family > 0x09000000U && family < 0x09600000U) ||
        (family > 0x09600000U && family <= 0x10ff0000U))
        return ShellUiHookBackend::PtraceIo;
    return ShellUiHookBackend::Unsupported;
}

enum class ShellUiHookStatus : std::int32_t {
    Success = 0,
    InvalidRequest = -1,
    UnsupportedFirmware = -2,
    AuthFailure = -3,
    AttachFailure = -4,
    ExpectedBytesMismatch = -5,
    WriteFailure = -6,
    VerifyFailure = -7,
    DetachFailure = -8,
    AuthRestoreFailure = -9,
};

#pragma pack(push, 1)
struct ShellUiHookRequest {
    std::uint64_t magic = kShellUiHookRequestMagic;
    std::uint32_t version = kShellUiHookProtocolVersion;
    std::int32_t pid = -1;
    std::uint64_t nonce = 0;
    std::uint64_t method_address = 0;
    std::uint64_t hook_address = 0;
    std::uint64_t original_call_address = 0;
    std::uint32_t patch_size = kShellUiHookPatchSize;
    std::uint32_t displaced_size = 0;
    std::uint8_t expected[kShellUiHookPatchSize]{};
    std::uint8_t desired[kShellUiHookPatchSize]{};
    std::uint32_t checksum = 0;
};

struct ShellUiHookAck {
    std::uint64_t magic = kShellUiHookAckMagic;
    std::uint32_t version = kShellUiHookProtocolVersion;
    std::int32_t pid = -1;
    std::uint64_t nonce = 0;
    std::int32_t status =
        static_cast<std::int32_t>(ShellUiHookStatus::InvalidRequest);
    std::int32_t read_rc = -1;
    std::int32_t write_rc = -1;
    std::uint8_t verified = 0;
    std::uint8_t restored = 0;
    std::uint8_t detached = 0;
    std::uint8_t auth_restored = 0;
    std::uint8_t phase =
        static_cast<std::uint8_t>(ShellUiHookAckPhase::Patch);
    std::uint8_t observed[kShellUiHookPatchSize]{};
    std::uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(sizeof(ShellUiHookRequest) == 92);
static_assert(sizeof(ShellUiHookAck) == 61);

inline std::uint32_t shellui_hook_checksum(
    const void* data,
    std::size_t size) noexcept {

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

inline std::uint32_t shellui_hook_request_checksum(
    const ShellUiHookRequest& request) noexcept {

    return shellui_hook_checksum(
        &request,
        offsetof(ShellUiHookRequest, checksum));
}

inline std::uint32_t shellui_hook_ack_checksum(
    const ShellUiHookAck& ack) noexcept {

    return shellui_hook_checksum(
        &ack,
        offsetof(ShellUiHookAck, checksum));
}

inline bool shellui_hook_is_eta_chain(
    const ShellUiHookRequest& request) noexcept {
    return std::memcmp(request.expected, kShellUiAbsoluteJumpPrefix,
                       sizeof(kShellUiAbsoluteJumpPrefix)) == 0;
}

inline bool shellui_user_address(std::uint64_t address) noexcept {
    return address >= 0x10000ULL && address < 0x0000800000000000ULL;
}

inline bool shellui_hook_request_common_is_valid(
    const ShellUiHookRequest& request, std::int32_t pid) noexcept {
    if (request.magic != kShellUiHookRequestMagic ||
        request.version != kShellUiHookProtocolVersion ||
        request.pid != pid || pid <= 0 || request.nonce == 0 ||
        !shellui_user_address(request.method_address) ||
        !shellui_user_address(request.method_address + kShellUiHookPatchSize - 1) ||
        !shellui_user_address(request.hook_address) ||
        request.patch_size != kShellUiHookPatchSize ||
        request.checksum != shellui_hook_request_checksum(request))
        return false;

    return true;
}

inline bool shellui_hook_request_is_probe(
    const ShellUiHookRequest& request) noexcept {
    return request.displaced_size == 0;
}

inline bool shellui_hook_request_is_main_thread_guard(
    const ShellUiHookRequest& request) noexcept {
    return request.displaced_size == 1 &&
        request.original_call_address == 0 &&
        request.hook_address == request.method_address;
}

inline bool shellui_hook_probe_request_is_valid(
    const ShellUiHookRequest& request, std::int32_t pid) noexcept {

    if (!shellui_hook_request_common_is_valid(request, pid) ||
        request.original_call_address != 0 ||
        !shellui_hook_request_is_probe(request))
        return false;

    for (std::size_t i = 0; i < kShellUiHookPatchSize; ++i)
        if (request.expected[i] != 0 || request.desired[i] != 0)
            return false;
    return true;
}

inline bool shellui_main_thread_guard_request_is_valid(
    const ShellUiHookRequest& request, std::int32_t pid) noexcept {

    if (!shellui_hook_request_common_is_valid(request, pid) ||
        !shellui_hook_request_is_main_thread_guard(request) ||
        request.expected[0] == 0xc3 ||
        request.desired[0] != 0xc3)
        return false;

    for (std::size_t i = 1; i < kShellUiHookPatchSize; ++i)
        if (request.desired[i] != request.expected[i])
            return false;

    return true;
}

inline bool shellui_hook_request_is_valid(
    const ShellUiHookRequest& request, std::int32_t pid) noexcept {

    if (!shellui_hook_request_common_is_valid(request, pid) ||
        !shellui_user_address(request.original_call_address) ||
        request.original_call_address == request.hook_address ||
        request.original_call_address == request.method_address ||
        request.displaced_size < 14 || request.displaced_size > 16 ||
        shellui_hook_request_is_probe(request) ||
        shellui_hook_request_is_main_thread_guard(request) ||
        std::memcmp(request.desired, kShellUiAbsoluteJumpPrefix, 6) != 0)
        return false;

    std::uint64_t target = 0;
    std::memcpy(&target, request.desired + 6, sizeof(target));
    if (target != request.hook_address)
        return false;

    if (shellui_hook_is_eta_chain(request)) {
        std::uint64_t previous = 0;
        std::memcpy(&previous, request.expected + 6, sizeof(previous));
        if (request.displaced_size != 14 ||
            previous != request.original_call_address)
            return false;
    } else {
        for (std::size_t i = 14; i < request.displaced_size; ++i)
            if (request.desired[i] != 0x90)
                return false;
    }

    for (std::size_t i = request.displaced_size;
         i < kShellUiHookPatchSize;
         ++i)
        if (request.desired[i] != request.expected[i])
            return false;
    return true;
}

inline ShellUiHookBackend shellui_hook_probe_backend(
    std::uint32_t sdk) noexcept {
    const std::uint32_t family = sdk & 0xffff0000U;
    if ((family >= 0x01000000U && family <= 0x08200000U) ||
        family == 0x09000000U)
        return ShellUiHookBackend::Mdbg;
    if (family >= 0x08300000U && family <= 0x10ff0000U)
        return ShellUiHookBackend::PtraceIo;
    return ShellUiHookBackend::Unsupported;
}

} // namespace common_fps
