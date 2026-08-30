/*
 * Common FPS v0.28b stable-source rebuild - PHU/libNineS ELF loader bridge
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Stable v0.28b carries two PHU extensions over the etaHEN-vendored
 * libNineS loader: Trace Continue and external-symbol relocation.
 */

#include <elf.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ps5/kernel.h>

/* Reuse libNineS's private loader context, PT_LOAD helper, constants and the
 * exact full elfldr_payload_args() implementation. Rename only its baseline
 * elfldr_load; the PHU-compatible implementation is below. */
#define elfldr_load commonfps_v028b_baseline_elfldr_load_unused
#include "elfldr.c"
#undef elfldr_load

static bool g_trace_continued = false;
static bool g_trace_stopped = false;
static unsigned g_import_resolved = 0;
static unsigned g_import_unresolved = 0;
static char g_first_unresolved[128];

/* Exact library search order recovered from stable v0.28b elfldr_load. */
static const char* const g_resolve_libraries[] = {
    "libkernel_sys.sprx",
    "libkernel.sprx",
    "libSceLibcInternal.sprx",
    "libmonosgen-2.0.sprx",
    "libmonosgen-2.0.0.sprx",
    "libScePad.sprx",
    "libSceUserService.sprx",
};

static int trace_continue_after_mmap(pid_t pid) {
    if (pt_continue(pid, SIGCONT) != 0)
        return -1;
    g_trace_continued = true;
    return 0;
}

static int trace_stop_before_io(pid_t pid) {
    if (!g_trace_continued || g_trace_stopped)
        return 0;
    if (kill(pid, SIGSTOP) != 0)
        return -1;
    if (waitpid(pid, 0, 0) < 0)
        return -1;
    g_trace_stopped = true;
    return 0;
}

static intptr_t resolve_external_symbol(pid_t pid, const char* name) {
    if (!name || !*name)
        return 0;

    for (size_t i = 0;
         i < sizeof(g_resolve_libraries) / sizeof(g_resolve_libraries[0]);
         ++i) {
        uint32_t handle = 0;
        const int rc = kernel_dynlib_handle(pid, g_resolve_libraries[i], &handle);
        if (rc < 0 || handle == 0)
            continue;
        const intptr_t address = kernel_dynlib_dlsym(pid, handle, name);
        if (address != 0)
            return address;
    }
    return 0;
}

static int apply_phu_relocations(elfldr_ctx_t* ctx, Elf64_Ehdr* ehdr,
                                 Elf64_Shdr* shdr) {
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        if (shdr[i].sh_type != SHT_RELA || shdr[i].sh_entsize == 0)
            continue;

        Elf64_Sym* dynsym = NULL;
        const char* dynstr = NULL;
        size_t dynsym_count = 0;
        size_t dynstr_size = 0;

        if (shdr[i].sh_link < ehdr->e_shnum) {
            Elf64_Shdr* symsec = &shdr[shdr[i].sh_link];
            if ((symsec->sh_type == SHT_DYNSYM || symsec->sh_type == SHT_SYMTAB) &&
                symsec->sh_entsize == sizeof(Elf64_Sym) &&
                symsec->sh_link < ehdr->e_shnum) {
                Elf64_Shdr* strsec = &shdr[symsec->sh_link];
                if (strsec->sh_type == SHT_STRTAB) {
                    dynsym = (Elf64_Sym*)(ctx->elf + symsec->sh_offset);
                    dynsym_count = symsec->sh_size / sizeof(Elf64_Sym);
                    dynstr = (const char*)(ctx->elf + strsec->sh_offset);
                    dynstr_size = strsec->sh_size;
                }
            }
        }

        Elf64_Rela* rela = (Elf64_Rela*)(ctx->elf + shdr[i].sh_offset);
        const size_t count = shdr[i].sh_size / sizeof(Elf64_Rela);

        for (size_t j = 0; j < count; ++j) {
            const uint32_t type = (uint32_t)ELF64_R_TYPE(rela[j].r_info);
            const uint32_t sym_index = (uint32_t)ELF64_R_SYM(rela[j].r_info);
            intptr_t* location =
                (intptr_t*)((uint8_t*)ctx->base_mirror + rela[j].r_offset);

            if (type == R_X86_64_RELATIVE) {
                *location = ctx->base_addr + rela[j].r_addend;
                continue;
            }

            /* Stable disassembly routes relocation types 1 and 6 through the
             * same dynsym/dynstr + kernel dlsym resolver. */
            if (type != R_X86_64_64 && type != R_X86_64_GLOB_DAT)
                continue;

            if (!dynsym || !dynstr || sym_index >= dynsym_count)
                return -1;

            const Elf64_Sym* sym = &dynsym[sym_index];
            if (sym->st_name >= dynstr_size)
                return -1;

            const char* name = dynstr + sym->st_name;
            const intptr_t resolved = resolve_external_symbol(ctx->pid, name);
            if (resolved == 0) {
                ++g_import_unresolved;
                if (g_first_unresolved[0] == '\0') {
                    strncpy(g_first_unresolved, name,
                            sizeof(g_first_unresolved) - 1);
                    g_first_unresolved[sizeof(g_first_unresolved) - 1] = '\0';
                }
                /* Stable PHU logs and leaves GOT=0. SR8A intentionally fails
                 * closed so a bad GOT is never executed in SceShellUI. */
                return -1;
            }

            ++g_import_resolved;
            *location = resolved + rela[j].r_addend;
        }
    }

    return 0;
}

intptr_t commonfps_v028b_elfldr_load(pid_t pid, uint8_t* elf) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf;
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf + ehdr->e_phoff);
    Elf64_Shdr* shdr = (Elf64_Shdr*)(elf + ehdr->e_shoff);
    elfldr_ctx_t ctx = {.elf = elf, .pid = pid};

    g_trace_continued = false;
    g_trace_stopped = false;
    g_import_resolved = 0;
    g_import_unresolved = 0;
    memset(g_first_unresolved, 0, sizeof(g_first_unresolved));

    size_t min_vaddr = (size_t)-1;
    size_t max_vaddr = 0;
    int error = 0;

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_vaddr < min_vaddr)
            min_vaddr = phdr[i].p_vaddr;
        if (max_vaddr < phdr[i].p_vaddr + phdr[i].p_memsz)
            max_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
    }

    min_vaddr = TRUNC_PG(min_vaddr);
    max_vaddr = ROUND_PG(max_vaddr);
    ctx.base_size = max_vaddr - min_vaddr;

    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    const int prot = PROT_READ | PROT_WRITE;
    if (ehdr->e_type == ET_DYN) {
        ctx.base_addr = 0;
    } else if (ehdr->e_type == ET_EXEC) {
        ctx.base_addr = min_vaddr;
        flags |= MAP_FIXED;
    } else {
        return 0;
    }

    ctx.base_mirror = malloc(ctx.base_size);
    if (!ctx.base_mirror)
        return 0;
    memset(ctx.base_mirror, 0, ctx.base_size);

    ctx.base_addr = pt_mmap(pid, ctx.base_addr, ctx.base_size,
                            prot, flags, -1, 0);
    if (ctx.base_addr == -1) {
        free(ctx.base_mirror);
        return 0;
    }

    /* v0.27d/v0.28b Trace Continue patch point #1. */
    if (trace_continue_after_mmap(pid) != 0)
        error = -1;

    for (int i = 0; i < ehdr->e_phnum && !error; ++i) {
        if (phdr[i].p_type == PT_LOAD)
            error = data_load(&ctx, &phdr[i]);
    }

    if (!error)
        error = apply_phu_relocations(&ctx, ehdr, shdr);

    /* v0.27d/v0.28b Trace Continue patch point #2. */
    if (!error && trace_stop_before_io(pid) != 0)
        error = -1;

    if (!error &&
        pt_copyin(ctx.pid, ctx.base_mirror, ctx.base_addr, ctx.base_size))
        error = -1;

    for (int i = 0; i < ehdr->e_phnum && !error; ++i) {
        if (phdr[i].p_type != PT_LOAD || phdr[i].p_memsz == 0)
            continue;

        if (phdr[i].p_flags & PF_X) {
            if (kernel_mprotect(pid,
                                ctx.base_addr + phdr[i].p_vaddr,
                                ROUND_PG(phdr[i].p_memsz),
                                PFLAGS(phdr[i].p_flags)))
                error = -1;
        } else {
            if (pt_mprotect(pid,
                            ctx.base_addr + phdr[i].p_vaddr,
                            ROUND_PG(phdr[i].p_memsz),
                            PFLAGS(phdr[i].p_flags)))
                error = -1;
        }
    }

    if (!error && pt_msync(pid, ctx.base_addr, ctx.base_size, MS_SYNC))
        error = -1;

    free(ctx.base_mirror);

    if (error) {
        if (g_trace_continued && !g_trace_stopped)
            (void)trace_stop_before_io(pid);
        (void)pt_munmap(pid, ctx.base_addr, ctx.base_size);
        return 0;
    }

    return ctx.base_addr + ehdr->e_entry;
}

intptr_t commonfps_v028b_elfldr_payload_args(pid_t pid) {
    return elfldr_payload_args(pid);
}

int commonfps_v028b_trace_continue_seen(void) {
    return g_trace_continued ? 1 : 0;
}

int commonfps_v028b_trace_stop_seen(void) {
    return g_trace_stopped ? 1 : 0;
}

unsigned commonfps_v028b_import_resolved_count(void) {
    return g_import_resolved;
}

unsigned commonfps_v028b_import_unresolved_count(void) {
    return g_import_unresolved;
}

const char* commonfps_v028b_first_unresolved(void) {
    return g_first_unresolved;
}
