/*
 * Common FPS for PS5
 * Stable v1.0.0 DMAP reader reconstruction for FW 9.60.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "v1_stable_dmap_backend.hpp"

#include <sys/types.h>
#include <sys/sysctl.h>

extern "C" {
unsigned long kernel_get_proc(int pid);
int kernel_copyout(unsigned long kaddr, void* uaddr, unsigned long len);
}

namespace common_fps::ps5 {
namespace {

/*
 * FW 9.60 values cross-checked against Team PHU's public
 * PS5-PHU-Trophy-System/src/phu_fw_offsets.c and the r11 donor.
 */
constexpr std::uint32_t kFw960Min = 0x09600000u;
constexpr std::uint32_t kFw960Max = 0x0960ffffu;

constexpr std::uintptr_t kProcVmspaceOffset = 0x200;
constexpr std::uintptr_t kVmspacePmapOffset = 0x1d8;
constexpr std::uintptr_t kPmapPml4Offset = 0x20;
constexpr std::uintptr_t kPmapCr3Offset = 0x28;

constexpr std::uintptr_t kPhysicalPageMask = 0x000ffffffffff000ull;
constexpr std::uintptr_t kFw9DmapFallback = 0xffff873b00000000ull;

bool plausible_kernel_dmap(std::uintptr_t value) noexcept {
    return (value & 0xffff000000000000ull) == 0xffff000000000000ull;
}

} // namespace

std::uint32_t V1StableDmapBackend::current_sdk_version() const {
    std::uint32_t sdk = 0;
    size_t size = sizeof(sdk);
    if (sysctlbyname("kern.sdk_version", &sdk, &size, nullptr, 0) != 0)
        return 0;
    if (size != sizeof(sdk))
        return 0;
    return sdk;
}

bool V1StableDmapBackend::kernel_read_u64(
    std::uintptr_t kernel_address,
    std::uint64_t& value) const {

    value = 0;
    return kernel_copyout(
               static_cast<unsigned long>(kernel_address),
               &value,
               sizeof(value)) == 0;
}

bool V1StableDmapBackend::prepare_process_translation(
    ProcessId pid,
    std::uintptr_t& cr3_physical) {

    dmap_base_ = 0;
    cr3_physical = 0;

    last_sdk_version_ = current_sdk_version();
    if (last_sdk_version_ < kFw960Min || last_sdk_version_ > kFw960Max)
        return false;

    const auto proc = static_cast<std::uintptr_t>(kernel_get_proc(pid));
    if (proc == 0)
        return false;

    std::uint64_t vmspace = 0;
    std::uint64_t pmap = 0;
    std::uint64_t pml4_virtual = 0;
    std::uint64_t cr3 = 0;

    if (!kernel_read_u64(proc + kProcVmspaceOffset, vmspace) || vmspace == 0)
        return false;

    if (!kernel_read_u64(
            static_cast<std::uintptr_t>(vmspace) + kVmspacePmapOffset,
            pmap) || pmap == 0) {
        return false;
    }

    if (!kernel_read_u64(
            static_cast<std::uintptr_t>(pmap) + kPmapPml4Offset,
            pml4_virtual)) {
        return false;
    }

    if (!kernel_read_u64(
            static_cast<std::uintptr_t>(pmap) + kPmapCr3Offset,
            cr3) || cr3 == 0) {
        return false;
    }

    const auto cr3_page =
        static_cast<std::uintptr_t>(cr3) & kPhysicalPageMask;
    cr3_physical = cr3_page;

    /*
     * PHU derives the direct-map base from pm_pml4 - pm_cr3. Keep that as the
     * primary path. The published 9.x fallback is used only if derivation is
     * unavailable or produces a non-kernel-canonical address.
     */
    if (pml4_virtual >= cr3_page && cr3_page != 0) {
        const auto derived =
            static_cast<std::uintptr_t>(pml4_virtual) - cr3_page;
        if (plausible_kernel_dmap(derived))
            dmap_base_ = derived;
    }

    if (dmap_base_ == 0)
        dmap_base_ = kFw9DmapFallback;

    return dmap_base_ != 0 && cr3_physical != 0;
}

bool V1StableDmapBackend::read_entry(
    std::uintptr_t physical_address,
    std::uint64_t& value) {

    if (dmap_base_ == 0)
        return false;

    value = 0;
    return kernel_copyout(
               static_cast<unsigned long>(dmap_base_ + physical_address),
               &value,
               sizeof(value)) == 0;
}

std::optional<common_fps::v1_stable::PageTranslation>
V1StableDmapBackend::translate(
    ProcessId pid,
    std::uintptr_t virtual_address) {

    std::uintptr_t cr3_physical = 0;
    if (!prepare_process_translation(pid, cr3_physical))
        return std::nullopt;

    return common_fps::v1_stable::translate_x86_64(
        *this,
        cr3_physical,
        virtual_address);
}

bool V1StableDmapBackend::copy_physical(
    std::uintptr_t physical_address,
    void* output,
    std::size_t size) {

    if (size == 0)
        return true;
    if (dmap_base_ == 0 || output == nullptr)
        return false;

    return kernel_copyout(
               static_cast<unsigned long>(dmap_base_ + physical_address),
               output,
               static_cast<unsigned long>(size)) == 0;
}

} // namespace common_fps::ps5
