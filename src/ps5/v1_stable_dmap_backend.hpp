/*
 * Common FPS for PS5
 * Stable v1.0.0 DMAP reader reconstruction for FW 9.60.
 *
 * Firmware structure offsets cross-checked against Team PHU's public
 * PS5-PHU-Trophy-System (MIT) and the PHU r11 donor DWARF/disassembly.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "common_fps/v1_stable_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace common_fps::ps5 {

class V1StableDmapBackend final
    : public common_fps::v1_stable::DmapReadBackend,
      private common_fps::v1_stable::PhysicalEntryReader {
public:
    V1StableDmapBackend() = default;

    std::optional<common_fps::v1_stable::PageTranslation>
    translate(ProcessId pid, std::uintptr_t virtual_address) override;

    bool copy_physical(
        std::uintptr_t physical_address,
        void* output,
        std::size_t size) override;

    [[nodiscard]] std::uint32_t last_sdk_version() const noexcept {
        return last_sdk_version_;
    }

    [[nodiscard]] std::uintptr_t last_dmap_base() const noexcept {
        return dmap_base_;
    }

private:
    bool read_entry(
        std::uintptr_t physical_address,
        std::uint64_t& value) override;

    bool prepare_process_translation(
        ProcessId pid,
        std::uintptr_t& cr3_physical);

    bool kernel_read_u64(
        std::uintptr_t kernel_address,
        std::uint64_t& value) const;

    std::uint32_t current_sdk_version() const;

    std::uint32_t last_sdk_version_ = 0;
    std::uintptr_t dmap_base_ = 0;
};

} // namespace common_fps::ps5
