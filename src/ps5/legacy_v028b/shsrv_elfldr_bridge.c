/*
 * Common FPS v0.28b source rebuild - shsrv ELF loader bridge
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This translation unit compiles the pinned ps5-payload-dev/shsrv v0.20
 * elfldr.c directly, then exposes only the two private primitives required by
 * the hardware-proven v0.28b-style remote-thread bootstrap.
 *
 * shsrv/elfldr.c is GPL-3.0-or-later, Copyright (C) 2024 John Tornblom.
 */

#include <stdint.h>
#include <sys/types.h>

/* Supplied through target_include_directories from the pinned dependency. */
#include "elfldr.c"

intptr_t commonfps_v028b_elfldr_load(pid_t pid, uint8_t* elf) {
    return elfldr_load(pid, elf, 0);
}

intptr_t commonfps_v028b_elfldr_payload_args(pid_t pid) {
    return elfldr_payload_args(pid);
}
