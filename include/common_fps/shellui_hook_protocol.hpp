/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace common_fps {

inline constexpr const char* kShellUiHookRequestPath =
    "/system_tmp/commonfps_stage8_2_hook_request.bin";
inline constexpr const char* kShellUiHookRequestTempPath =
    "/system_tmp/commonfps_stage8_2_hook_request.tmp";
inline constexpr const char* kShellUiHookAckPath =
    "/system_tmp/commonfps_stage8_2_hook_ack.bin";
inline constexpr const char* kShellUiHookAckTempPath =
    "/system_tmp/commonfps_stage8_2_hook_ack.tmp";

inline constexpr std::uint64_t kShellUiHookRequestMagic =
    0x325145524b484643ULL; /* "CFHKREQ2" */
inline constexpr std::uint64_t kShellUiHookAckMagic =
    0x324b43414b484643ULL; /* "CFHKACK2" */
inline constexpr std::uint32_t kShellUiHookProtocolVersion = 2;
inline constexpr std::size_t kShellUiHookPatchSize = 16;

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
    std::uint64_t trampoline_address = 0;
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

} // namespace common_fps
