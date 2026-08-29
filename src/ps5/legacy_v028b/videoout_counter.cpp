#include "videoout_counter.hpp"

#include "proc_rw_v960.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

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

} // namespace

std::optional<std::uintptr_t>
resolve_videoout_counter(pid_t pid) noexcept {
    if (pid <= 0)
        return std::nullopt;

    std::uint32_t handle = 0;
    if (kernel_dynlib_handle(pid, kVideoOutModule, &handle) != 0 || handle == 0)
        return std::nullopt;

    const std::uintptr_t module_base = static_cast<std::uintptr_t>(
        kernel_dynlib_mapbase_addr(pid, handle));
    if (module_base == 0)
        return std::nullopt;

    std::array<std::uint8_t, kProbeTableSize> table{};
    if (!proc_read(
            pid,
            module_base + kProbeTableOffset,
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
            return std::nullopt;
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
