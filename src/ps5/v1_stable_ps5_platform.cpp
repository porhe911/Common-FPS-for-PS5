/*
 * Common FPS for PS5
 * Stable v1.0.0 PS5 platform adapter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "v1_stable_ps5_platform.hpp"

#include "common_fps/v1_stable_memory.hpp"

namespace common_fps::ps5 {

std::optional<ProcessId> V1StablePs5Platform::find_game_process() {
    return base_.find_game_process();
}

bool V1StablePs5Platform::process_alive(ProcessId pid) {
    return base_.process_alive(pid);
}

std::optional<ModuleInfo>
V1StablePs5Platform::find_module(ProcessId pid, const char* module_name) {
    return base_.find_module(pid, module_name);
}

bool V1StablePs5Platform::read_memory(
    ProcessId pid,
    std::uintptr_t address,
    void* out,
    std::size_t size) {

    return common_fps::v1_stable::proc_read_dmap(
        dmap_,
        pid,
        address,
        out,
        size);
}

std::uint64_t V1StablePs5Platform::monotonic_us() {
    return base_.monotonic_us();
}

void V1StablePs5Platform::sleep_ms(unsigned milliseconds) {
    base_.sleep_ms(milliseconds);
}

} // namespace common_fps::ps5
