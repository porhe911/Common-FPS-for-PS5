/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/shellui_code_read.hpp"
#include "shellui_mdbg_io.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace common_fps;
namespace {
constexpr std::uint64_t kAuth = 0x4800000000010003ULL;
constexpr std::uintptr_t kMethod = 0x82072b550ULL;
const std::array<unsigned char, 16> kCode{
    0x55,0x48,0x89,0xe5,0x48,0x83,0xec,0x20,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
enum class Mode { Complete, Chunked, StatusError, NoProgress, Oversize,
                  SyscallError, SetupFailure, RestoreFailure };
struct State {
    std::uint64_t auth = kAuth;
    std::array<unsigned char, 16> caps{};
    Mode mode = Mode::Complete;
    std::array<unsigned char, 16> memory = kCode;
    unsigned calls = 0, auth_restores = 0, caps_restores = 0;
} state;
void reset(Mode mode) { state = {}; state.caps.fill(0x5a); state.mode = mode; }
bool zero(const std::array<unsigned char, 16>& b) {
    return b == std::array<unsigned char, 16>{};
}
struct Command { unsigned long type, op; };
struct Args { int pid; unsigned long source, destination, length; };
struct Reply { int status; unsigned long length; };
}

extern "C" std::uint64_t kernel_get_ucred_authid(pid_t) { return state.auth; }
extern "C" std::int32_t kernel_set_ucred_authid(pid_t, std::uint64_t auth) {
    if (auth == kAuth) {
        ++state.auth_restores;
        if (state.mode == Mode::RestoreFailure) return -1;
    }
    state.auth = auth;
    return 0;
}
extern "C" std::int32_t kernel_get_ucred_caps(pid_t, std::uint8_t* caps) {
    std::memcpy(caps, state.caps.data(), 16); return 0;
}
extern "C" std::int32_t kernel_set_ucred_caps(pid_t, const std::uint8_t* caps) {
    if (caps[0] == 0x5a) ++state.caps_restores;
    if (state.mode == Mode::SetupFailure && caps[0] == 0xff) return -1;
    std::memcpy(state.caps.data(), caps, 16); return 0;
}

// Exercise the production checked reader against a simulated kernel reply.
extern "C" long syscall(long number, ...) noexcept {
    assert(number == 573);
    assert(state.auth == 0x4800000000000006ULL && state.caps[0] == 0xff);
    ++state.calls;
    va_list ap; va_start(ap, number);
    const auto* command = va_arg(ap, const Command*);
    const auto* args = va_arg(ap, const Args*);
    auto* reply = va_arg(ap, Reply*);
    va_end(ap);
    assert(command->type == 1 && (command->op == 0x12 || command->op == 0x13));
    const bool read = command->op == 0x12;
    const auto address = read ? args->source : args->destination;
    assert(args->pid == 58 && address >= kMethod);
    if (!read) assert(args->length == 1); // only the guard byte may be written
    const auto offset = address - kMethod;
    assert(offset < kCode.size() && args->length <= kCode.size() - offset);
    if (state.mode == Mode::SyscallError) { errno = EFAULT; return -1; }
    reply->status = state.mode == Mode::StatusError ? EFAULT : 0;
    reply->length = state.mode == Mode::NoProgress ? 0 :
        (state.mode == Mode::Oversize ? args->length + 1 :
         (state.mode == Mode::Chunked ? std::min(4UL, args->length) : args->length));
    const auto count = std::min(reply->length, args->length);
    if (read)
        std::memcpy(reinterpret_cast<void*>(args->destination), state.memory.data() + offset, count);
    else
        std::memcpy(state.memory.data() + offset, reinterpret_cast<const void*>(args->source), count);
    return 0;
}

static void test_checked_live_reader() {
    for (Mode mode : {Mode::Complete, Mode::Chunked, Mode::StatusError,
                      Mode::NoProgress, Mode::Oversize, Mode::SyscallError,
                      Mode::SetupFailure, Mode::RestoreFailure}) {
        reset(mode);
        const auto caps = state.caps;
        std::array<unsigned char, 16> out{}; out.fill(0xee);
        const int rc = common_fps::ps5::shellui_mdbg_read_exact(58, kMethod, out.data(), out.size());
        const bool success = mode == Mode::Complete || mode == Mode::Chunked;
        assert((rc == 0) == success);
        assert(success ? out == kCode : zero(out));
        assert(state.caps == caps && state.caps_restores == 1 && state.auth_restores == 1);
        if (mode != Mode::RestoreFailure) assert(state.auth == kAuth);
        if (mode == Mode::Chunked) assert(state.calls == 4);
        if (mode == Mode::SetupFailure) assert(state.calls == 0);
    }
}

static void test_fw1160_actual_efault_fallback() {
    reset(Mode::Complete);
    unsigned ptrace_calls = 0;
    std::array<unsigned char, 16> out{};
    const auto result = read_shellui_code(0x11600005U, ShellUiHookBackend::PtraceIo,
        out.data(), out.size(),
        [&](void* p, std::size_t size) { ++ptrace_calls; std::memset(p, 0xee, size); return EFAULT; },
        [&](void* p, std::size_t size) { return common_fps::ps5::shellui_mdbg_read_exact(58, kMethod, p, size); });
    assert(result.rc == 0 && result.primary_rc == EFAULT && result.fallback_rc == 0);
    assert(result.fallback_attempted && ptrace_calls == 1 && state.calls == 1 && out == kCode);
    assert(state.auth == kAuth && state.caps[0] == 0x5a);

    reset(Mode::StatusError); // syscall returns zero but reported read failed
    const auto failed = read_shellui_code(0x11600005U, ShellUiHookBackend::PtraceIo,
        out.data(), out.size(), [](void*, std::size_t) { return EFAULT; },
        [&](void* p, std::size_t size) { return common_fps::ps5::shellui_mdbg_read_exact(58, kMethod, p, size); });
    assert(failed.rc == EFAULT && failed.fallback_attempted && zero(out));
}

static void test_checked_one_byte_writer() {
    for (Mode mode : {Mode::Complete, Mode::Chunked, Mode::StatusError,
                      Mode::NoProgress, Mode::Oversize, Mode::SyscallError,
                      Mode::SetupFailure, Mode::RestoreFailure}) {
        reset(mode);
        const int rc = common_fps::ps5::shellui_mdbg_write_byte(58, kMethod, 0xc3);
        const bool success = mode == Mode::Complete || mode == Mode::Chunked;
        assert((rc == 0) == success);
        if (success) assert(state.memory[0] == 0xc3);
        for (std::size_t i = 1; i < kCode.size(); ++i)
            assert(state.memory[i] == kCode[i]);
        assert(state.caps[0] == 0x5a && state.caps_restores == 1 && state.auth_restores == 1);
        if (mode != Mode::RestoreFailure) assert(state.auth == kAuth);
    }
}

static void test_no_unrelated_fallback() {
    for (auto sdk : {0x09600005U, 0x10600005U, 0x11000005U, 0x12000005U}) {
        unsigned fallback_calls = 0;
        std::array<unsigned char, 16> out{};
        const auto r = read_shellui_code(sdk, ShellUiHookBackend::PtraceIo,
            out.data(), out.size(), [](void*, std::size_t) { return EFAULT; },
            [&](void*, std::size_t) { ++fallback_calls; return 0; });
        assert(r.rc == EFAULT && !r.fallback_attempted && fallback_calls == 0);
    }
    for (int error : {0, EPERM, EINVAL, -1}) {
        unsigned fallback_calls = 0;
        std::array<unsigned char, 16> out{};
        const auto r = read_shellui_code(0x11600005U, ShellUiHookBackend::PtraceIo,
            out.data(), out.size(), [&](void*, std::size_t) { return error; },
            [&](void*, std::size_t) { ++fallback_calls; return 0; });
        assert(r.rc == error && !r.fallback_attempted && fallback_calls == 0);
    }
    unsigned calls = 0;
    std::array<unsigned char, 16> out{};
    const auto unsupported = read_shellui_code(0x11600005U, ShellUiHookBackend::Unsupported,
        out.data(), out.size(), [&](void*, std::size_t) { ++calls; return 0; },
        [&](void*, std::size_t) { ++calls; return 0; });
    assert(unsupported.rc != 0 && calls == 0);
}

int main() {
    test_checked_live_reader();
    test_checked_one_byte_writer();
    test_fw1160_actual_efault_fallback();
    test_no_unrelated_fallback();
    std::cout << "FW1160 EFAULT recovery, exact MDBG I/O and credential restoration: PASS\n";
}
