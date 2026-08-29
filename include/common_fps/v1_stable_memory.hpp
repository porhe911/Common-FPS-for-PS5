/*
 * Common FPS for PS5
 * Stable v1.0.0 memory-read behavior reconstructed from PHU r11.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "common_fps/platform.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace common_fps::v1_stable {

struct PageTranslation {
    std::uintptr_t physical = 0;
    std::size_t page_size = 0;
};

/* Read one 64-bit x86-64 page-table entry by physical address. */
class PhysicalEntryReader {
public:
    virtual ~PhysicalEntryReader() = default;
    virtual bool read_entry(
        std::uintptr_t physical_address,
        std::uint64_t& value) = 0;
};

/*
 * Four-level x86-64 page-table translation used by the PHU DMAP reader.
 * Supports ordinary 4 KiB mappings plus PS-bit 2 MiB and 1 GiB mappings.
 */
std::optional<PageTranslation> translate_x86_64(
    PhysicalEntryReader& reader,
    std::uintptr_t cr3_physical,
    std::uintptr_t virtual_address) noexcept;

/*
 * Host-testable boundary around the historical PHU DMAP reader.
 *
 * The real PS5 implementation of stable v1.0.0 used:
 *   translate(pid, virtual_address, &page_size)
 *   kernel_copyout(g_dmap_base + physical_address, ...)
 *
 * It did NOT pt_attach()/pt_detach() the game for every VideoOut read.
 */
class DmapReadBackend {
public:
    virtual ~DmapReadBackend() = default;

    virtual std::optional<PageTranslation>
    translate(ProcessId pid, std::uintptr_t virtual_address) = 0;

    virtual bool copy_physical(
        std::uintptr_t physical_address,
        void* output,
        std::size_t size) = 0;
};

/*
 * Reconstructed prw::proc_read() chunking semantics.
 *
 * A read may cross 4 KiB / 2 MiB / 1 GiB mappings. The historical routine
 * translates each current virtual address, limits the copy to the remaining
 * bytes in that mapping, then repeats until the requested length is complete.
 */
bool proc_read_dmap(
    DmapReadBackend& backend,
    ProcessId pid,
    std::uintptr_t virtual_address,
    void* output,
    std::size_t size) noexcept;

} // namespace common_fps::v1_stable
