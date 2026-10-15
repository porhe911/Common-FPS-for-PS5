/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_service.hpp"
#include <iostream>
#include <unistd.h>

// Integration fixture uses the real controller HTTP worker and persistent store.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    common_fps::SettingsStore store(argv[1]);
    if (!common_fps::ps5::start_settings_service(store)) return 1;
    std::cout << "ready\n" << std::flush;
    for (;;) pause();
}
