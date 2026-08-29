/*
 * Common FPS for PS5
 * Stable v1.0.0 memory-read behavior reconstructed from PHU r11.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "common_fps/v1_stable_memory.hpp"

#include <algorithm>
#include <cstdint>

namespace common_fps::v1_stable {

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
