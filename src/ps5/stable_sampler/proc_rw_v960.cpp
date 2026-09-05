/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "proc_rw_v960.hpp"

#include <algorithm>
#include <cstdint>
#include <sys/sysctl.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace common_fps::ps5::stable_sampler {
namespace {

constexpr std::uint32_t kFw960 = 0x09600000U;
constexpr std::uintptr_t kAllprocFromKdata = 0x02755D50ULL;
constexpr std::uintptr_t kProcPid = 0xBCULL;
constexpr std::uintptr_t kProcVmspace = 0x200ULL;
constexpr std::uintptr_t kProcNext = 0x00ULL;
constexpr std::uintptr_t kVmspacePmap = 0x1D8ULL;
constexpr std::uintptr_t kPmapPml4Virtual = 0x20ULL;
constexpr std::uintptr_t kPmapPml4Physical = 0x28ULL;
constexpr std::uintptr_t kFixedDmapBase = 0xFFFF873B00000000ULL;

constexpr std::uint64_t kPresent = 1ULL;
constexpr std::uint64_t kPageSizeBit = 1ULL << 7;
constexpr std::uint64_t kMask4K = 0x000FFFFFFFFFF000ULL;
constexpr std::uint64_t kMask2M = 0x000FFFFFFFE00000ULL;
constexpr std::uint64_t kMask1G = 0x000FFFFFC0000000ULL;

constexpr std::size_t kPage4K = 0x1000ULL;
constexpr std::size_t kPage2M = 0x200000ULL;
constexpr std::size_t kPage1G = 0x40000000ULL;
constexpr unsigned kMaxProcWalk = 1024U;

std::uintptr_t g_dmap_base = 0;

bool read_kernel(
    std::uintptr_t address,
    void* out,
    std::size_t size) noexcept {
    return address != 0 && out != nullptr && size != 0 &&
        kernel_copyout(address, out, size) == 0;
}

template <typename T>
bool read_kernel_value(std::uintptr_t address, T& out) noexcept {
    out = {};
    return read_kernel(address, &out, sizeof(out));
}

std::uintptr_t find_proc(pid_t pid) noexcept {
    if (pid <= 0 || !firmware_is_960() || KERNEL_ADDRESS_DATA_BASE == 0)
        return 0;

    std::uintptr_t current = 0;
    if (!read_kernel_value(
            static_cast<std::uintptr_t>(KERNEL_ADDRESS_DATA_BASE) +
                kAllprocFromKdata,
            current)) {
        return 0;
    }

    for (unsigned i = 0; current != 0 && i < kMaxProcWalk; ++i) {
        std::uint32_t candidate_pid = 0;
        if (!read_kernel_value(current + kProcPid, candidate_pid))
            return 0;

        if (candidate_pid == static_cast<std::uint32_t>(pid))
            return current;

        std::uintptr_t next = 0;
        if (!read_kernel_value(current + kProcNext, next) || next == current)
            return 0;
        current = next;
    }

    return 0;
}

std::uintptr_t choose_dmap(
    std::uintptr_t pml4_virtual,
    std::uintptr_t pml4_physical) noexcept {

    if (g_dmap_base != 0)
        return g_dmap_base;

    if (pml4_virtual > pml4_physical &&
        pml4_physical < (1ULL << 40)) {
        const std::uintptr_t candidate = pml4_virtual - pml4_physical;
        if ((candidate >> 48) != 0)
            g_dmap_base = candidate;
    }

    if (g_dmap_base == 0)
        g_dmap_base = kFixedDmapBase;

    return g_dmap_base;
}

bool read_page_entry(std::uintptr_t address, std::uint64_t& entry) noexcept {
    return read_kernel_value(address, entry) && (entry & kPresent) != 0;
}

} // namespace

std::uint32_t firmware_sdk_version() noexcept {
    std::uint32_t sdk = 0;
    std::size_t sdk_len = sizeof(sdk);
    if (sysctlbyname("kern.sdk_version", &sdk, &sdk_len, nullptr, 0) != 0 ||
        sdk_len != sizeof(sdk)) {
        return 0;
    }
    return sdk;
}

bool firmware_is_960() noexcept {
    return (firmware_sdk_version() & 0xFFFF0000U) == kFw960;
}

std::uintptr_t dmap_base() noexcept {
    return g_dmap_base != 0 ? g_dmap_base : kFixedDmapBase;
}

std::uintptr_t translate(
    pid_t pid,
    std::uintptr_t remote,
    std::size_t* page_size) noexcept {

    if (page_size)
        *page_size = 0;

    const std::uintptr_t proc = find_proc(pid);
    if (proc == 0)
        return 0;

    std::uintptr_t vmspace = 0;
    std::uintptr_t pmap = 0;
    std::uintptr_t pml4_virtual = 0;
    std::uintptr_t pml4_physical = 0;

    if (!read_kernel_value(proc + kProcVmspace, vmspace) || vmspace == 0)
        return 0;
    if (!read_kernel_value(vmspace + kVmspacePmap, pmap) || pmap == 0)
        return 0;
    if (!read_kernel_value(pmap + kPmapPml4Virtual, pml4_virtual) ||
        pml4_virtual == 0) {
        return 0;
    }
    if (!read_kernel_value(pmap + kPmapPml4Physical, pml4_physical) ||
        pml4_physical == 0) {
        return 0;
    }

    const std::uintptr_t dmap = choose_dmap(pml4_virtual, pml4_physical);
    if (dmap == 0)
        return 0;

    const std::size_t pml4_index = (remote >> 39) & 0x1FFULL;
    const std::size_t pdpt_index = (remote >> 30) & 0x1FFULL;
    const std::size_t pd_index = (remote >> 21) & 0x1FFULL;
    const std::size_t pt_index = (remote >> 12) & 0x1FFULL;

    std::uint64_t pml4e = 0;
    if (!read_page_entry(pml4_virtual + pml4_index * 8ULL, pml4e))
        return 0;

    std::uint64_t pdpte = 0;
    if (!read_page_entry(
            dmap + (pml4e & kMask4K) + pdpt_index * 8ULL,
            pdpte)) {
        return 0;
    }

    if ((pdpte & kPageSizeBit) != 0) {
        if (page_size)
            *page_size = kPage1G;
        return static_cast<std::uintptr_t>(pdpte & kMask1G) |
            (remote & (kPage1G - 1ULL));
    }

    std::uint64_t pde = 0;
    if (!read_page_entry(
            dmap + (pdpte & kMask4K) + pd_index * 8ULL,
            pde)) {
        return 0;
    }

    if ((pde & kPageSizeBit) != 0) {
        if (page_size)
            *page_size = kPage2M;
        return static_cast<std::uintptr_t>(pde & kMask2M) |
            (remote & (kPage2M - 1ULL));
    }

    std::uint64_t pte = 0;
    if (!read_page_entry(
            dmap + (pde & kMask4K) + pt_index * 8ULL,
            pte)) {
        return 0;
    }

    if (page_size)
        *page_size = kPage4K;
    return static_cast<std::uintptr_t>(pte & kMask4K) |
        (remote & (kPage4K - 1ULL));
}

bool proc_read(
    pid_t pid,
    std::uintptr_t remote,
    void* local,
    std::size_t size) noexcept {

    if (size == 0)
        return true;
    if (pid <= 0 || remote == 0 || local == nullptr)
        return false;

    auto* output = static_cast<std::uint8_t*>(local);
    std::size_t remaining = size;

    while (remaining != 0) {
        std::size_t page_size = 0;
        const std::uintptr_t physical = translate(pid, remote, &page_size);
        if (physical == 0 || page_size == 0)
            return false;

        const std::size_t page_offset =
            static_cast<std::size_t>(remote & (page_size - 1ULL));
        const std::size_t chunk =
            std::min(remaining, page_size - page_offset);

        if (kernel_copyout(dmap_base() + physical, output, chunk) != 0)
            return false;

        remote += chunk;
        output += chunk;
        remaining -= chunk;
    }

    return true;
}

} // namespace common_fps::ps5::stable_sampler
