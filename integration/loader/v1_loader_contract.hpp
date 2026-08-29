/*
 * Common FPS for PS5
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace common_fps::loader {

struct LoadedImage {
    std::uintptr_t entry = 0;
    std::uintptr_t base = 0;
};

class Backend {
public:
    virtual ~Backend() = default;

    // Historical v0.27a: do not start the loader before ShellUI Scene exists.
    virtual bool wait_for_shellui_scene() = 0;

    // Historical v0.27b/c: one continuous trace/loader session.
    virtual bool begin_loader_session() = 0;

    /*
     * Load the renderer image first.
     *
     * The PS5 backend for this operation must implement the recovered v1.0.0
     * shsrv Trace Continue behavior inside image loading:
     *   reserve remote image -> temporarily continue target while doing local
     *   ELF preparation -> synchronously re-stop before later remote/protection
     *   operations. No intermediate detach/re-attach is allowed.
     */
    virtual std::optional<LoadedImage> load_renderer_image(
        const std::uint8_t* elf,
        std::size_t size) = 0;

    // Historical v0.28a hardware result: this phase must be retained.
    virtual std::uintptr_t prepare_payload_args() = 0;

    // Equivalent to shsrv's register preparation after image + payload args.
    virtual bool prepare_exec(
        std::uintptr_t entry,
        std::uintptr_t payload_args) = 0;

    /*
     * Finish the same loader session. On the real backend this is the final
     * detach/resume path; there is no second independent continue step here.
     */
    virtual bool end_loader_session() = 0;
};

bool install_renderer_v1_compatible(
    Backend& backend,
    const std::uint8_t* elf,
    std::size_t size);

} // namespace common_fps::loader
