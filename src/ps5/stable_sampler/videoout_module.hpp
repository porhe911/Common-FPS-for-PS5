/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace common_fps::ps5::stable_sampler {

struct VideoOutModuleResult {
    std::uintptr_t base = 0;
    bool auth_read = false;
    bool auth_changed = false;
    bool auth_restore_attempted = false;
    bool auth_restored = false;
    long size_query_rc = -1;
    long fill_query_rc = -1;
    long last_info_rc = -1;
    std::size_t requested_count = 0;
    std::size_t returned_count = 0;
    unsigned info_successes = 0;
};

/*
 * Resolves libSceVideoOut.sprx with a short self Auth-ID window. The original
 * Auth ID is restored before this function returns and before any DMAP read.
 */
[[nodiscard]] VideoOutModuleResult
find_videoout_module(pid_t pid) noexcept;

} // namespace common_fps::ps5::stable_sampler
