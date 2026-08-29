#include "videoout_counter.hpp"

#include "proc_rw_v960.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace common_fps::legacy_v028b {
namespace {

constexpr char kVideoOutModule[] = "libSceVideoOut.sprx";
constexpr std::uintptr_t kProbeTableOffset = 0x34980ULL;
constexpr std::size_t kProbeTableSize = 0xA8ULL;
constexpr std::size_t kProbeEntrySize = 0x18ULL;
constexpr std::size_t kProbeEntryCount = kProbeTableSize / kProbeEntrySize;
constexpr std::uintptr_t kCounterOffset = 0x768ULL;

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

std::optional<std::uintptr_t> find_videoout_base(pid_t pid) noexcept {
    const std::uintptr_t self_ucred = kernel_get_proc_ucred(getpid());
    if (self_ucred == 0)
        return std::nullopt;

    std::uint64_t saved_auth = 0;
    if (kernel_copyout(self_ucred + kAuthIdOffset, &saved_auth, sizeof(saved_auth)) != 0)
        return std::nullopt;

    const std::uint64_t debug_auth = kDebuggerAuthId;
    if (kernel_copyin(&debug_auth, self_ucred + kAuthIdOffset, sizeof(debug_auth)) != 0)
        return std::nullopt;

    std::optional<std::uintptr_t> result;
    std::size_t count = 0;
    const long rc_size = syscall(kSysDlGetList, pid, nullptr, 0, &count);

    if ((rc_size == 0 || rc_size == kExpectedSizeQueryRc) &&
        count > 0 && count < 1024) {
        auto* handles = static_cast<std::uintptr_t*>(
            std::calloc(count, sizeof(std::uintptr_t)));
        if (handles != nullptr) {
            std::size_t returned = count;
            const long rc_fill = syscall(
                kSysDlGetList, pid, handles, count, &returned);
            if (rc_fill == 0 && returned <= count) {
                for (std::size_t i = 0; i < returned; ++i) {
                    ModuleInfoCompat info{};
                    const long rc_info = syscall(
                        kSysDlGetInfo2, pid, 1, handles[i], &info);
                    if (rc_info != 0)
                        continue;
                    info.filename[kModuleNameLength - 1] = '\0';
                    if (std::strcmp(info.filename, kVideoOutModule) == 0 &&
                        info.sections[0].vaddr != 0) {
                        result = static_cast<std::uintptr_t>(
                            info.sections[0].vaddr);
                        break;
                    }
                }
            }
            std::free(handles);
        }
    }

    // This restore is mandatory.  All DMAP work and periodic counter reads run
    // with the original Auth ID, matching the hardware-proven SR5 path.
    if (kernel_copyin(&saved_auth, self_ucred + kAuthIdOffset, sizeof(saved_auth)) != 0)
        return std::nullopt;

    return result;
}

} // namespace

std::optional<std::uintptr_t>
resolve_videoout_counter(pid_t pid) noexcept {
    if (pid <= 0)
        return std::nullopt;

    const auto module_base = find_videoout_base(pid);
    if (!module_base)
        return std::nullopt;

    std::array<std::uint8_t, kProbeTableSize> table{};
    if (!proc_read(
            pid,
            *module_base + kProbeTableOffset,
            table.data(),
            table.size())) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < kProbeEntryCount; ++i) {
        const auto* entry = table.data() + i * kProbeEntrySize;

        std::uint32_t enabled = 0;
        std::uint64_t pointer = 0;
        std::memcpy(&enabled, entry + 0x00, sizeof(enabled));
        std::memcpy(&pointer, entry + 0x08, sizeof(pointer));

        if (enabled == 0 || pointer == 0)
            continue;

        std::uint64_t root = 0;
        if (!proc_read(
                pid,
                static_cast<std::uintptr_t>(pointer),
                &root,
                sizeof(root)) ||
            root == 0) {
            continue;
        }

        return static_cast<std::uintptr_t>(root) + kCounterOffset;
    }

    return std::nullopt;
}

bool read_videoout_counter(
    pid_t pid,
    std::uintptr_t counter_address,
    std::uint32_t& value) noexcept {
    value = 0;
    return pid > 0 && counter_address != 0 &&
           proc_read(pid, counter_address, &value, sizeof(value));
}

} // namespace common_fps::legacy_v028b
