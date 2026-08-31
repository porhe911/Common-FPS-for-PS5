/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"

#include <cstdio>
#include <unistd.h>

namespace {

constexpr const char* kMarker = "/system_tmp/commonfps_shellui.pid";

void write_online_marker() {
    FILE* fp = std::fopen(kMarker, "w");
    if (!fp)
        return;
    std::fprintf(fp, "%d\n", getpid());
    std::fclose(fp);
}

} // namespace

extern "C" int main() {
    using namespace common_fps::ps5::shellui;

    if (!initialize_runtime())
        return 1;

    if (!initialize_receiver())
        return 2;

    write_online_marker();

    for (;;) {
        apply_latest_state();
        usleep(10000);
    }

    return 0;
}
