/*
 * Common FPS for PS5
 * Stable v1.0.0 memory-read behavior reconstructed from PHU r11.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "common_fps/v1_stable_memory.hpp"

#include <algorithm>
#include <cstdint>

namespace common_fps::v1_stable {
namespace {

constexpr std::uint64_t kPresent = 1ull << 0;
constexpr std::uint64_t kPageSizeBit = 1ull << 7;

constexpr std::uintptr_t kTableAddressMask = 0x000ffffffffff000ull;
constexpr std::uintptr_t kPage1GiBAddressMask = 0x000fffffc0000000ull;
constexpr std::uintptr_t kPage2MiBAddressMask = 0x000fffffffe00000ull;
constexpr std::uintptr_t kPage4KiBAddressMask = 0x000ffffffffff000ull;

constexpr std::size_t kPage1GiB = 0x40000000ull;
constexpr std::size_t kPage2MiB = 0x00200000ull;
constexpr std::size_t kPage4KiB = 0x00001000ull;

constexpr std::uintptr_t table_index(
    std::uintptr_t virtual_address,
    unsigned shift) noexcept {
    return (virtual_address >> shift) & 0x1ffull;
}

bool read_table_entry(
    PhysicalEntryReader& reader,
    std::uintptr_t table_physical,
    std::uintptr_t index,
    std::uint64_t& entry) noexcept {

    return reader.read_entry(table_physical + index * sizeof(std::uint64_t), entry);
}

} // namespace

std::optional<PageTranslation> translate_x86_64(
    PhysicalEntryReader& reader,
    std::uintptr_t cr3_physical,
    std::uintptr_t virtual_address) noexcept {

    // CR3 low twelve bits are PCID / control bits, not part of the PML4 PA.
    std::uintptr_t table = cr3_physical & kTableAddressMask;
    if (table == 0)
        return std::nullopt;

    std::uint64_t entry = 0;

    // PML4E
    if (!read_table_entry(reader, table, table_index(virtual_address, 39), entry) ||
        (entry & kPresent) == 0) {
        return std::nullopt;
    }
    table = static_cast<std::uintptr_t>(entry) & kTableAddressMask;

    // PDPTE: PS=1 means a 1 GiB mapping.
    if (!read_table_entry(reader, table, table_index(virtual_address, 30), entry) ||
        (entry & kPresent) == 0) {
        return std::nullopt;
    }
    if ((entry & kPageSizeBit) != 0) {
        const auto physical_base =
            static_cast<std::uintptr_t>(entry) & kPage1GiBAddressMask;
        return PageTranslation{
            physical_base + (virtual_address & (kPage1GiB - 1)),
            kPage1GiB,
        };
    }
    table = static_cast<std::uintptr_t>(entry) & kTableAddressMask;

    // PDE: PS=1 means a 2 MiB mapping.
    if (!read_table_entry(reader, table, table_index(virtual_address, 21), entry) ||
        (entry & kPresent) == 0) {
        return std::nullopt;
    }
    if ((entry & kPageSizeBit) != 0) {
        const auto physical_base =
            static_cast<std::uintptr_t>(entry) & kPage2MiBAddressMask;
        return PageTranslation{
            physical_base + (virtual_address & (kPage2MiB - 1)),
            kPage2MiB,
        };
    }
    table = static_cast<std::uintptr_t>(entry) & kTableAddressMask;

    // PTE: ordinary 4 KiB mapping.
    if (!read_table_entry(reader, table, table_index(virtual_address, 12), entry) ||
        (entry & kPresent) == 0) {
        return std::nullopt;
    }

    const auto physical_base =
        static_cast<std::uintptr_t>(entry) & kPage4KiBAddressMask;
    return PageTranslation{
        physical_base + (virtual_address & (kPage4KiB - 1)),
        kPage4KiB,
    };
}

bool proc_read_dmap(
    DmapReadBackend& backend,
    ProcessId pid,
    std::uintptr_t virtual_address,
    void* output,
    std::size_t size) noexcept {

    if (size == 0)
        return true;
    if (!output)
        return false;

    auto* destination = static_cast<std::uint8_t*>(output);
    std::size_t remaining = size;

    while (remaining != 0) {
        const auto translation = backend.translate(pid, virtual_address);
        if (!translation || translation->physical == 0 ||
            translation->page_size == 0) {
            return false;
        }

        const std::size_t page_size = translation->page_size;
        const std::size_t offset_in_page =
            static_cast<std::size_t>(virtual_address & (page_size - 1));
        const std::size_t until_page_end = page_size - offset_in_page;
        const std::size_t chunk = std::min(remaining, until_page_end);

        if (chunk == 0 ||
            !backend.copy_physical(
                translation->physical,
                destination,
                chunk)) {
            return false;
        }

        virtual_address += chunk;
        destination += chunk;
        remaining -= chunk;
    }

    return true;
}

} // namespace common_fps::v1_stable
