/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "videoout_module.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace common_fps::ps5::stable_sampler {
namespace {

constexpr char kVideoOutModule[] = "libSceVideoOut.sprx";
constexpr std::uint64_t kDebuggerAuthId = 0x4800000000000006ULL;
constexpr std::uintptr_t kAuthIdOffset = 0x58ULL;
constexpr long kSysDlGetList = 0x217;
constexpr long kSysDlGetInfo2 = 0x2cd;
constexpr long kExpectedSizeQueryRc = 12;

constexpr std::size_t kModuleNameLength = 128;
constexpr std::size_t kSandboxPathLength = 1024;
constexpr std::size_t kMaxSections = 4;
constexpr std::size_t kFingerprintLength = 20;

struct ModuleSectionCompat {
    std::uint64_t vaddr;
    std::uint32_t size;
    std::uint32_t prot;
};

struct ModuleInfoCompat {
    char filename[kModuleNameLength];
    std::uint64_t handle;
    std::uint8_t unknown0[32];
    std::uint64_t init;
    std::uint64_t fini;
    std::uint64_t eh_frame_hdr;
    std::uint64_t eh_frame_hdr_sz;
    std::uint64_t eh_frame;
    std::uint64_t eh_frame_sz;
    ModuleSectionCompat sections[kMaxSections];
    std::uint8_t unknown7[1176];
    std::uint8_t fingerprint[kFingerprintLength];
    std::uint32_t unknown8;
    char libname[kModuleNameLength];
    std::uint32_t unknown9;
    char sandboxed_path[kSandboxPathLength];
    std::uint64_t sdk_version;
};

} // namespace

VideoOutModuleResult find_videoout_module(pid_t pid) noexcept {
    VideoOutModuleResult result{};
    if (pid <= 0)
        return result;

    const std::uintptr_t self_ucred = kernel_get_proc_ucred(getpid());
    if (self_ucred == 0)
        return result;

    std::uint64_t saved_auth = 0;
    if (kernel_copyout(
            self_ucred + kAuthIdOffset,
            &saved_auth,
            sizeof(saved_auth)) != 0) {
        return result;
    }
    result.auth_read = true;

    const std::uint64_t debug_auth = kDebuggerAuthId;
    if (kernel_copyin(
            &debug_auth,
            self_ucred + kAuthIdOffset,
            sizeof(debug_auth)) != 0) {
        return result;
    }
    result.auth_changed = true;

    result.size_query_rc = syscall(
        kSysDlGetList,
        pid,
        nullptr,
        0,
        &result.requested_count);

    if ((result.size_query_rc == 0 ||
         result.size_query_rc == kExpectedSizeQueryRc) &&
        result.requested_count > 0 &&
        result.requested_count < 1024) {
        auto* handles = static_cast<std::uintptr_t*>(std::calloc(
            result.requested_count,
            sizeof(std::uintptr_t)));

        if (handles) {
            result.returned_count = result.requested_count;
            result.fill_query_rc = syscall(
                kSysDlGetList,
                pid,
                handles,
                result.requested_count,
                &result.returned_count);

            if (result.fill_query_rc == 0 &&
                result.returned_count <= result.requested_count) {
                for (std::size_t i = 0;
                     i < result.returned_count;
                     ++i) {
                    ModuleInfoCompat info{};
                    result.last_info_rc = syscall(
                        kSysDlGetInfo2,
                        pid,
                        1,
                        handles[i],
                        &info);
                    if (result.last_info_rc != 0)
                        continue;

                    ++result.info_successes;
                    info.filename[kModuleNameLength - 1] = '\0';
                    if (std::strcmp(info.filename, kVideoOutModule) == 0 &&
                        info.sections[0].vaddr != 0) {
                        result.base = static_cast<std::uintptr_t>(
                            info.sections[0].vaddr);
                        break;
                    }
                }
            }

            std::free(handles);
        }
    }

    /* Restoring the original Auth ID before DMAP access is mandatory. */
    result.auth_restore_attempted = true;
    result.auth_restored = kernel_copyin(
        &saved_auth,
        self_ucred + kAuthIdOffset,
        sizeof(saved_auth)) == 0;

    if (!result.auth_restored)
        result.base = 0;

    return result;
}

} // namespace common_fps::ps5::stable_sampler
