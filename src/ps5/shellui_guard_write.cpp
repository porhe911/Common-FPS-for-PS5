/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shellui_guard_write.hpp"
#include "shellui_mdbg_io.hpp"
#include "common_fps/shellui_hook_protocol.hpp"
#include <cerrno>
#include <sys/mman.h>
extern "C" {
#include <ps5/kernel.h>
}

namespace common_fps::ps5 {
int shellui_write_guard_byte(pid_t pid, std::uintptr_t address,
                            std::uint8_t byte,
                            ShellUiGuardWriteState& state) noexcept {
    if (pid <= 0 || !shellui_user_address(address) ||
        (state.original_protection >= 0 &&
         (state.pid != pid || state.address != address)))
        return EINVAL;
    state.protect_rc = state.mdbg_write_rc = state.restore_protection_rc = -1;
    state.protection_restored = false;
    int current = kernel_get_vmem_protection(pid, address, 1);
    if (current < 0) {
        // A rollback must still try to recover its saved protection after a
        // failed query. The initial attempt cannot touch an unknown mapping.
        if (state.original_protection >= 0) {
            state.restore_protection_rc = kernel_mprotect(
                pid, address, 1, state.original_protection);
            state.protection_restored = state.restore_protection_rc == 0 &&
                kernel_get_vmem_protection(pid, address, 1) == state.original_protection;
        }
        if (!state.protection_restored)
            return EIO;
        current = state.original_protection;
    }
    if (current > 0xff || !(current & PROT_EXEC))
        return EIO;
    if (state.original_protection < 0) {
        state.pid = pid;
        state.address = address;
        state.original_protection = current;
    }
    const int writable = state.original_protection | PROT_READ | PROT_WRITE;
    if (current != state.original_protection && current != writable)
        return EIO;

    state.protect_rc = current == writable ? 0 :
        kernel_mprotect(pid, address, 1, writable);
    if (state.protect_rc == 0 &&
        kernel_get_vmem_protection(pid, address, 1) == writable)
        state.mdbg_write_rc = shellui_mdbg_write_byte(pid, address, byte);

    // Restore even if setup failed after touching the VM entry. The original
    // byte of protection flags, including SDK-specific bits, is preserved.
    state.restore_protection_rc = kernel_mprotect(
        pid, address, 1, state.original_protection);
    state.protection_restored = state.restore_protection_rc == 0 &&
        kernel_get_vmem_protection(pid, address, 1) == state.original_protection;
    if (!state.protection_restored)
        return -1001;
    if (state.protect_rc != 0)
        return -1000;
    return state.mdbg_write_rc == -1 ? EIO : state.mdbg_write_rc;
}
} // namespace common_fps::ps5
