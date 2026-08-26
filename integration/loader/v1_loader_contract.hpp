/*
 * Common FPS for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace common_fps::loader {

class Backend {
public:
    virtual ~Backend() = default;

    // v0.27a: required or overlay may never appear.
    virtual bool wait_for_shellui_scene() = 0;

    // v0.27b/c: one continuous loader session.
    virtual bool begin_loader_session() = 0;

    // v0.28a: must not be removed.
    virtual std::uintptr_t prepare_payload_args() = 0;

    virtual bool load_renderer(
        const std::uint8_t* elf,
        std::size_t size,
        std::uintptr_t payload_args) = 0;

    virtual bool continue_target() = 0;
    virtual bool end_loader_session() = 0;
};

bool install_renderer_v1_compatible(
    Backend& backend,
    const std::uint8_t* elf,
    std::size_t size);

} // namespace common_fps::loader
