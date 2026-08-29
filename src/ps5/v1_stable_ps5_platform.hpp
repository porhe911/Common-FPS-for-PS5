/*
 * Common FPS for PS5
 * Stable v1.0.0 PS5 platform adapter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "common_fps/platform.hpp"
#include "ps5_platform.hpp"
#include "v1_stable_dmap_backend.hpp"

namespace common_fps::ps5 {

/*
 * Process/module discovery and timing stay on the existing etaHEN-backed
 * Ps5Platform implementation. Only remote game-memory reads are replaced by
 * the reconstructed PHU DMAP path used by hardware-stable v1.0.0.
 */
class V1StablePs5Platform final : public Platform {
public:
    std::optional<ProcessId> find_game_process() override;
    bool process_alive(ProcessId pid) override;

    std::optional<ModuleInfo>
    find_module(ProcessId pid, const char* module_name) override;

    bool read_memory(
        ProcessId pid,
        std::uintptr_t address,
        void* out,
        std::size_t size) override;

    std::uint64_t monotonic_us() override;
    void sleep_ms(unsigned milliseconds) override;

    [[nodiscard]] const V1StableDmapBackend& dmap_backend() const noexcept {
        return dmap_;
    }

private:
    Ps5Platform base_;
    V1StableDmapBackend dmap_;
};

} // namespace common_fps::ps5
