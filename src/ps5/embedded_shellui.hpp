/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>

namespace common_fps::ps5::embedded {

/*
 * Generated at PS5 build time from the source-built ShellUI companion ELF.
 * The definitions live in a generated .cpp file and are linked directly into
 * the controller, so end users only need one .plugin (etaHEN) or one .elf
 * (standalone/YouTube Jailbreak autoload).
 */
extern const unsigned char kShellUIElf[];
extern const std::size_t kShellUIElfSize;

} // namespace common_fps::ps5::embedded
