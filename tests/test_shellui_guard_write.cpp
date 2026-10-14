/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shellui_guard_write.hpp"
#include "shellui_mdbg_io.hpp"
#include <array>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <sys/mman.h>

using namespace common_fps::ps5;
namespace {
constexpr pid_t kPid = 59;
constexpr std::uintptr_t kAddress = 0x82ba53550ULL;
constexpr int kOriginal = 0x80 | PROT_READ | PROT_EXEC;
constexpr int kWritable = kOriginal | PROT_WRITE;
enum class Mode { Normal, SnapshotError, NonExecutable, SetupError,
                  SetupReadbackError, WriteError, RestoreError,
                  RestoreReadbackError, SetupAndRestoreError };
struct KernelState {
    int protection = kOriginal;
    int baseline = kOriginal;
    Mode mode = Mode::Normal;
    unsigned queries = 0, setups = 0, restores = 0, writes = 0;
    unsigned query_errors_remaining = 0;
    std::array<std::uint8_t, 16> code{};
} kernel;
void reset(Mode mode = Mode::Normal) {
    kernel = {};
    kernel.mode = mode;
    kernel.code.fill(0x90);
    kernel.code[0] = 0x55;
    if (mode == Mode::NonExecutable) kernel.protection = PROT_READ;
}
void unchanged_tail() {
    for (std::size_t i = 1; i < kernel.code.size(); ++i)
        assert(kernel.code[i] == 0x90);
}
}

extern "C" int kernel_get_vmem_protection(pid_t pid, intptr_t address, size_t length) {
    assert(pid == kPid && std::uintptr_t(address) == kAddress && length == 1);
    ++kernel.queries;
    if (kernel.query_errors_remaining > 0) {
        --kernel.query_errors_remaining;
        return -1;
    }
    return kernel.mode == Mode::SnapshotError ? -1 : kernel.protection;
}
extern "C" int kernel_mprotect(pid_t pid, intptr_t address, size_t length, int prot) {
    assert(pid == kPid && std::uintptr_t(address) == kAddress && length == 1);
    if (prot == kernel.baseline) {
        ++kernel.restores;
        if (kernel.mode == Mode::RestoreError || kernel.mode == Mode::SetupAndRestoreError)
            return -1;
        if (kernel.mode != Mode::RestoreReadbackError) kernel.protection = prot;
        return 0;
    }
    assert(prot == (kernel.baseline | PROT_READ | PROT_WRITE));
    ++kernel.setups;
    if (kernel.mode != Mode::SetupReadbackError) kernel.protection = prot;
    // Model an error after the VM entry was touched. Restoration is required.
    return kernel.mode == Mode::SetupError || kernel.mode == Mode::SetupAndRestoreError ? -1 : 0;
}
namespace common_fps::ps5 {
int shellui_mdbg_write_byte(pid_t pid, std::uintptr_t address, std::uint8_t byte) noexcept {
    assert(pid == kPid && address == kAddress);
    assert(kernel.protection == (kernel.baseline | PROT_READ | PROT_WRITE));
    ++kernel.writes;
    kernel.code[0] = byte;
    // A debugger may copy data before reporting an error. The controller must
    // verify/rollback rather than equating a nonzero return with no change.
    return kernel.mode == Mode::WriteError ? EFAULT : 0;
}
}

static void test_guard_and_exact_original_protection() {
    reset();
    ShellUiGuardWriteState state{};
    assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) == 0);
    assert(state.original_protection == kOriginal && state.protection_restored);
    assert(state.protect_rc == 0 && state.mdbg_write_rc == 0 && state.restore_protection_rc == 0);
    assert(kernel.protection == kOriginal && kernel.code[0] == 0xc3);
    assert(kernel.setups == 1 && kernel.restores == 1 && kernel.writes == 1);
    unchanged_tail();

    reset();
    kernel.protection = kernel.baseline = kWritable;
    state = {};
    assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) == 0);
    assert(kernel.setups == 0 && kernel.protection == kWritable && state.protection_restored);
    assert(state.original_protection == kWritable);
}

static void test_setup_and_write_errors_restore() {
    for (auto mode : {Mode::SnapshotError, Mode::NonExecutable,
                      Mode::SetupError, Mode::SetupReadbackError, Mode::WriteError}) {
        reset(mode);
        ShellUiGuardWriteState state{};
        assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) != 0);
        if (mode == Mode::SnapshotError || mode == Mode::NonExecutable) {
            assert(kernel.setups == 0 && kernel.writes == 0 && kernel.restores == 0);
        } else {
            assert(kernel.protection == kOriginal && state.protection_restored);
            assert(kernel.restores == 1);
            if (mode != Mode::WriteError) assert(kernel.writes == 0);
        }
        unchanged_tail();
    }
}

static void test_rollback_retains_initial_protection() {
    for (auto mode : {Mode::WriteError, Mode::RestoreError, Mode::RestoreReadbackError}) {
        reset(mode);
        ShellUiGuardWriteState state{};
        assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) != 0);
        assert(kernel.code[0] == 0xc3 && state.original_protection == kOriginal);
        if (mode != Mode::WriteError) {
            assert(!state.protection_restored && kernel.protection == kWritable);
        }
        kernel.mode = Mode::Normal;
        assert(shellui_write_guard_byte(kPid, kAddress, 0x55, state) == 0);
        assert(kernel.code[0] == 0x55 && kernel.protection == kOriginal);
        assert(state.original_protection == kOriginal && state.protection_restored);
        assert(kernel.writes == 2 && kernel.restores == 2);
        unchanged_tail();
    }
}

static void test_bad_scope_and_unrelated_protection() {
    reset();
    ShellUiGuardWriteState state{};
    assert(shellui_write_guard_byte(0, kAddress, 0xc3, state) == EINVAL);
    assert(shellui_write_guard_byte(kPid, 1, 0xc3, state) == EINVAL);
    assert(kernel.queries == 0);
    assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) == 0);
    const auto queries = kernel.queries;
    assert(shellui_write_guard_byte(kPid + 1, kAddress, 0x55, state) == EINVAL);
    assert(shellui_write_guard_byte(kPid, kAddress + 1, 0x55, state) == EINVAL);
    assert(kernel.queries == queries && kernel.writes == 1);
    kernel.protection = kOriginal | 0x10;
    assert(shellui_write_guard_byte(kPid, kAddress, 0x55, state) != 0);
    assert(kernel.writes == 1 && kernel.protection == (kOriginal | 0x10));
}

static void test_setup_failure_and_query_failure_during_rollback() {
    reset(Mode::SetupAndRestoreError);
    ShellUiGuardWriteState state{};
    assert(shellui_write_guard_byte(kPid, kAddress, 0xc3, state) != 0);
    assert(kernel.code[0] == 0x55 && kernel.writes == 0);
    assert(kernel.protection == kWritable && !state.protection_restored);
    // Unchanged code does not imply restored VM protection. A rollback must
    // retain the initial flags even when its first protection query fails.
    kernel.mode = Mode::Normal;
    kernel.query_errors_remaining = 1;
    assert(shellui_write_guard_byte(kPid, kAddress, 0x55, state) == 0);
    assert(state.original_protection == kOriginal && state.protection_restored);
    assert(kernel.protection == kOriginal && kernel.code[0] == 0x55);
    assert(kernel.restores == 3 && kernel.writes == 1);
    unchanged_tail();
}

int main() {
    test_guard_and_exact_original_protection();
    test_setup_and_write_errors_restore();
    test_rollback_retains_initial_protection();
    test_bad_scope_and_unrelated_protection();
    test_setup_failure_and_query_failure_during_rollback();
    std::cout << "FW1160 guard byte, VM protection and rollback recovery: PASS\n";
}
