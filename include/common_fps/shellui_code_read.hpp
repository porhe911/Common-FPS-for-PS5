/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "common_fps/firmware_runtime_policy.hpp"
#include "common_fps/shellui_hook_protocol.hpp"
#include <cerrno>
#include <cstddef>
#include <cstring>

namespace common_fps {

struct DebuggerReadChunk {
    int syscall_rc = -1;
    int status = 0;
    std::size_t bytes = 0;
};

// A successful syscall alone is insufficient: require status and byte count.
template <typename ReadChunk>
int debugger_read_exact(void* destination, std::size_t size,
                        ReadChunk&& read_chunk) noexcept {
    if (!destination || size == 0)
        return EINVAL;
    auto* bytes = static_cast<unsigned char*>(destination);
    std::memset(bytes, 0, size);
    std::size_t offset = 0;
    while (offset < size) {
        const auto result = read_chunk(bytes + offset, offset, size - offset);
        int error = result.syscall_rc != 0 ? result.syscall_rc : result.status;
        if (error == 0 && (result.bytes == 0 || result.bytes > size - offset))
            error = EIO;
        if (error != 0) {
            std::memset(bytes, 0, size);
            return error;
        }
        offset += result.bytes;
    }
    return 0;
}

struct ShellUiCodeReadResult {
    int rc = -1;
    int primary_rc = -1;
    int fallback_rc = -1;
    bool fallback_attempted = false;
};

template <typename PtraceRead, typename MdbgRead>
ShellUiCodeReadResult read_shellui_code(
    std::uint32_t sdk, ShellUiHookBackend backend,
    void* destination, std::size_t size,
    PtraceRead&& ptrace_read, MdbgRead&& mdbg_read) noexcept {
    ShellUiCodeReadResult result{};
    if (!destination || size == 0) {
        result.rc = EINVAL;
        return result;
    }
    if (backend == ShellUiHookBackend::Unsupported) {
        result.rc = ENOTSUP;
        return result;
    }
    result.primary_rc = backend == ShellUiHookBackend::Mdbg
        ? mdbg_read(destination, size) : ptrace_read(destination, size);
    result.rc = result.primary_rc;
    // RC3 hardware evidence: PT_IO returned EFAULT at the ShellUI JIT address.
    // Only that error on this exact firmware family permits a read fallback.
    if (backend == ShellUiHookBackend::PtraceIo &&
        firmware_is_fw1160(sdk) && result.primary_rc == EFAULT) {
        result.fallback_attempted = true;
        std::memset(destination, 0, size);
        result.fallback_rc = mdbg_read(destination, size);
        result.rc = result.fallback_rc;
    }
    if (result.rc != 0)
        std::memset(destination, 0, size);
    return result;
}

} // namespace common_fps
