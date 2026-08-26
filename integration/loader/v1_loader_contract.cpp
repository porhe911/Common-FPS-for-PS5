/*
 * Common FPS for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "v1_loader_contract.hpp"

namespace common_fps::loader {

bool install_renderer_v1_compatible(
    Backend& b,
    const std::uint8_t* elf,
    std::size_t size) {

    if (!elf || !size)
        return false;

    if (!b.wait_for_shellui_scene())
        return false;

    if (!b.begin_loader_session())
        return false;

    const auto args = b.prepare_payload_args();
    if (!args) {
        b.end_loader_session();
        return false;
    }

    if (!b.load_renderer(elf, size, args)) {
        b.end_loader_session();
        return false;
    }

    if (!b.continue_target()) {
        b.end_loader_session();
        return false;
    }

    return b.end_loader_session();
}

} // namespace common_fps::loader
