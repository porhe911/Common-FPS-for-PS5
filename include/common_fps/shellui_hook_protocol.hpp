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
    "/system_tmp/commonfps_stage8_3_hook_request.bin";
inline constexpr const char* kShellUiHookRequestTempPath =
    "/system_tmp/commonfps_stage8_3_hook_request.tmp";
inline constexpr const char* kShellUiHookAckPath =
    "/system_tmp/commonfps_stage8_3_hook_ack.bin";
inline constexpr const char* kShellUiHookAckTempPath =
    "/system_tmp/commonfps_stage8_3_hook_ack.tmp";

inline constexpr std::uint64_t kShellUiHookRequestMagic =
    0x335145524b484643ULL; /* "CFHKREQ3" */
inline constexpr std::uint64_t kShellUiHookAckMagic =
    0x334b43414b484643ULL; /* "CFHKACK3" */
inline constexpr std::uint32_t kShellUiHookProtocolVersion = 3;
inline constexpr std::size_t kShellUiHookPatchSize = 16;
inline constexpr std::uint8_t kShellUiAbsoluteJumpPrefix[6] = {
    0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
};

enum class ShellUiHookBackend { Unsupported, Mdbg, PtraceIo };

inline ShellUiHookBackend shellui_hook_backend(
    std::uint32_t sdk, bool eta_chain) noexcept {
    const std::uint32_t family = sdk & 0xffff0000U;
    if (family >= 0x03000000U && family <= 0x08200000U)
        return ShellUiHookBackend::Mdbg;
    /* New stopped-chain path: requires a separate 9.60 hardware regression. */
    if (family == 0x09600000U && eta_chain)
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
    std::uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(sizeof(ShellUiHookRequest) == 92);
static_assert(sizeof(ShellUiHookAck) == 44);

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

inline bool shellui_hook_request_is_valid(
    const ShellUiHookRequest& request, std::int32_t pid) noexcept {
    const auto user_address = [](std::uint64_t address) {
        return address >= 0x10000ULL && address < 0x0000800000000000ULL;
    };
    if (request.magic != kShellUiHookRequestMagic ||
        request.version != kShellUiHookProtocolVersion ||
        request.pid != pid || pid <= 0 || request.nonce == 0 ||
        !user_address(request.method_address) ||
        !user_address(request.method_address + kShellUiHookPatchSize - 1) ||
        !user_address(request.hook_address) ||
        !user_address(request.original_call_address) ||
        request.original_call_address == request.hook_address ||
        request.original_call_address == request.method_address ||
        request.patch_size != kShellUiHookPatchSize ||
        request.displaced_size < 14 || request.displaced_size > 16 ||
        request.checksum != shellui_hook_request_checksum(request) ||
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

    for (std::size_t i = request.displaced_size; i < kShellUiHookPatchSize; ++i)
        if (request.desired[i] != request.expected[i])
            return false;
    return true;
}

} // namespace common_fps
