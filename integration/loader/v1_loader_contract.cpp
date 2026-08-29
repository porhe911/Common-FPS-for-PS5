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

    const auto image = b.load_renderer_image(elf, size);
    if (!image || image->entry == 0) {
        b.end_loader_session();
        return false;
    }

    const auto args = b.prepare_payload_args();
    if (!args) {
        b.end_loader_session();
        return false;
    }

    if (!b.prepare_exec(image->entry, args)) {
        b.end_loader_session();
        return false;
    }

    return b.end_loader_session();
}

} // namespace common_fps::loader
