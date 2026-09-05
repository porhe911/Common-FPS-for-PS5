/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <sys/types.h>

namespace common_fps::ps5::stable_sampler {

struct ProcessLookupResult {
    pid_t pid = -1;
    int size_query_rc = -1;
    int data_query_rc = -1;
    int saved_errno = 0;
    std::size_t bytes = 0;
    unsigned records = 0;
    unsigned malformed_records = 0;
};

/*
 * FW 9.60 KERN_PROC lookup used by the hardware-proven sampler.
 * Returns the last non-self process whose tdname is exactly "eboot.bin".
 */
[[nodiscard]] ProcessLookupResult find_game_process_sysctl() noexcept;

} // namespace common_fps::ps5::stable_sampler
