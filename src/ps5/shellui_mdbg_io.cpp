/*
 * Common FPS for PS5, Copyright (C) 2026 porhe911
 * MDBG ABI / credential-window reference: ps5-payload-dev/sdk v0.41,
 * crt/mdbg.c and crt/syscall.h, Copyright (C) 2024 John Tornblom.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shellui_mdbg_io.hpp"
#include "common_fps/shellui_code_read.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <unistd.h>
extern "C" {
#include <ps5/kernel.h>
}

namespace common_fps::ps5 {
namespace {
constexpr int kMdbgSyscall = 573; // pinned SDK v0.41 crt/syscall.h
constexpr std::uint64_t kCoredumpAuth = 0x4800000000000006ULL;
struct MdbgCommand { unsigned long type; unsigned long operation; };
struct MdbgArguments {
    int pid;
    unsigned long source;
    unsigned long destination;
    unsigned long length;
};
struct MdbgReply { int status; unsigned long length; };
static_assert(sizeof(MdbgCommand) == 16);
static_assert(sizeof(MdbgArguments) == 32 && offsetof(MdbgArguments, source) == 8);
static_assert(sizeof(MdbgReply) == 16 && offsetof(MdbgReply, length) == 8);
int transfer_exact(bool read, pid_t pid, std::uintptr_t address,
                   void* buffer, std::size_t size) noexcept {
    if (pid <= 0 || !buffer || size == 0 || size > kShellUiHookPatchSize ||
        (!read && size != 1) ||
        !shellui_user_address(address) || !shellui_user_address(address + size - 1))
        return EINVAL;
    if (read)
        std::memset(buffer, 0, size);
    const pid_t self = getpid();
    const std::uint64_t original_auth = kernel_get_ucred_authid(self);
    std::array<unsigned char, 16> original_caps{};
    if (original_auth == 0 || kernel_get_ucred_caps(self, original_caps.data()) != 0)
        return EIO;
    std::array<unsigned char, 16> privileged_caps{};
    privileged_caps.fill(0xff);
    int result = EIO;
    if (kernel_set_ucred_authid(self, kCoredumpAuth) == 0 &&
        kernel_set_ucred_caps(self, privileged_caps.data()) == 0) {
        result = debugger_transfer_exact(size,
            [&](std::size_t offset, std::size_t remaining) {
                const auto local = reinterpret_cast<unsigned long>(buffer) + offset;
                const auto remote = address + offset;
                const MdbgCommand command{1, read ? 0x12UL : 0x13UL};
                MdbgArguments args{pid, read ? remote : local,
                    read ? local : remote, remaining};
                MdbgReply reply{};
                errno = 0;
                const int rc = syscall(kMdbgSyscall, &command, &args, &reply);
                const int error = rc == -1 ? (errno != 0 ? errno : EIO) : rc;
                return DebuggerReadChunk{error, reply.status, reply.length};
            });
    }
    // Always attempt both restores, including a partially failed setup.
    const bool auth_set = kernel_set_ucred_authid(self, original_auth) == 0;
    const bool caps_set = kernel_set_ucred_caps(self, original_caps.data()) == 0;
    std::array<unsigned char, 16> restored_caps{};
    const bool auth_restored = auth_set && kernel_get_ucred_authid(self) == original_auth;
    const bool caps_restored = caps_set &&
        kernel_get_ucred_caps(self, restored_caps.data()) == 0 &&
        restored_caps == original_caps;
    if (!auth_restored || !caps_restored)
        result = EIO;
    if (read && result != 0)
        std::memset(buffer, 0, size);
    return result;
}
} // namespace

int shellui_mdbg_read_exact(pid_t pid, std::uintptr_t address,
                           void* destination, std::size_t size) noexcept {
    return transfer_exact(true, pid, address, destination, size);
}

int shellui_mdbg_write_byte(pid_t pid, std::uintptr_t address,
                           std::uint8_t byte) noexcept {
    return transfer_exact(false, pid, address, &byte, 1);
}
} // namespace common_fps::ps5
