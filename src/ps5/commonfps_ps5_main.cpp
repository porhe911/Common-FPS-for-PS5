/*
 * Common FPS for PS5 - RC5 etaHEN wrapper/no-op safety probe
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdio>

extern "C" int main() {
    FILE* f = std::fopen("/data/CommonFPS_RC5_noop.log", "a");
    if (f) {
        std::fprintf(f, "RC5 START minimal no-op\n");
        std::fprintf(f, "RC5 RETURN 0; NO fork / NO sysctl / NO kernel / NO ptrace / NO auth / NO inject\n");
        std::fclose(f);
    }
    return 0;
}
