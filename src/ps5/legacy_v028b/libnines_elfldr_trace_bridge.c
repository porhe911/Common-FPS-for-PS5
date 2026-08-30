/*
 * Common FPS v0.28b stable-source rebuild - libNineS Trace Continue bridge
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The exact stable v0.28b ELF was built from libNineS/src/elfldr.c and carries
 * two small loader changes recovered from disassembly and the v0.27d notes:
 *
 *   remote mmap -> PT_CONTINUE while tracer remains attached
 *   local ELF work -> SIGSTOP + waitpid -> first PT_IO/copyin
 *
 * This translation unit compiles the pinned etaHEN-vendored libNineS loader
 * directly and wraps only pt_mmap/pt_copyin while elfldr_load() is active.
 * elfldr_payload_args() runs with the normal libNineS primitives afterwards.
 */

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Pre-include the real declarations before macro-interposing the loader TU. */
#include "pt.h"

static bool g_commonfps_trace_active = false;
static bool g_commonfps_trace_continued = false;
static bool g_commonfps_trace_stopped = false;

static intptr_t commonfps_trace_pt_mmap(pid_t pid, intptr_t addr, size_t len,
                                        int prot, int flags, int fd, off_t off);
static int commonfps_trace_pt_copyin(pid_t pid, const void* buf,
                                     intptr_t addr, size_t len);

#define pt_mmap   commonfps_trace_pt_mmap
#define pt_copyin commonfps_trace_pt_copyin
#include "elfldr.c"
#undef pt_copyin
#undef pt_mmap

static int commonfps_trace_stop(pid_t pid) {
    if (!g_commonfps_trace_continued || g_commonfps_trace_stopped)
        return 0;

    if (kill(pid, SIGSTOP) != 0)
        return -1;
    if (waitpid(pid, 0, 0) < 0)
        return -1;

    g_commonfps_trace_stopped = true;
    return 0;
}

static intptr_t commonfps_trace_pt_mmap(pid_t pid, intptr_t addr, size_t len,
                                        int prot, int flags, int fd, off_t off) {
    const intptr_t result = pt_mmap(pid, addr, len, prot, flags, fd, off);

    /* The first successful mmap inside elfldr_load is the stable patch point. */
    if (g_commonfps_trace_active && !g_commonfps_trace_continued && result != -1) {
        if (pt_continue(pid, SIGCONT) != 0)
            return -1;
        g_commonfps_trace_continued = true;
    }
    return result;
}

static int commonfps_trace_pt_copyin(pid_t pid, const void* buf,
                                     intptr_t addr, size_t len) {
    /* First PT_IO after local parse/relocation: stop exactly before the copy. */
    if (g_commonfps_trace_active && g_commonfps_trace_continued &&
        !g_commonfps_trace_stopped) {
        if (commonfps_trace_stop(pid) != 0)
            return -1;
    }
    return pt_copyin(pid, buf, addr, len);
}

intptr_t commonfps_v028b_elfldr_load(pid_t pid, uint8_t* elf) {
    g_commonfps_trace_active = true;
    g_commonfps_trace_continued = false;
    g_commonfps_trace_stopped = false;

    const intptr_t result = elfldr_load(pid, elf);

    /* Defensive parity: never leave the tracee running on an early loader exit. */
    if (g_commonfps_trace_continued && !g_commonfps_trace_stopped)
        (void)commonfps_trace_stop(pid);

    g_commonfps_trace_active = false;
    return result;
}

intptr_t commonfps_v028b_elfldr_payload_args(pid_t pid) {
    return elfldr_payload_args(pid);
}

int commonfps_v028b_trace_continue_seen(void) {
    return g_commonfps_trace_continued ? 1 : 0;
}

int commonfps_v028b_trace_stop_seen(void) {
    return g_commonfps_trace_stopped ? 1 : 0;
}
