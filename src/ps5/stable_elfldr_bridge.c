/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Source-only bridge around the pinned ps5-payload-dev/shsrv v0.20 loader.
 * It restores the two loader properties observed in the stable v0.28b
 * binary:
 *
 *   1. temporarily continue ShellUI while the local ELF image is prepared,
 *      then synchronously stop it before remote writes resume;
 *   2. resolve R_X86_64_64/R_X86_64_GLOB_DAT imports used by the embedded
 *      source-built renderer.
 *
 * shsrv/elfldr.c remains GPL-3.0-or-later, Copyright (C) 2024 John Tornblom.
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

/*
 * This intentionally includes the pinned implementation so its private
 * loader context, payload-args builder and PT_LOAD helper can be reused.
 * Do not also compile shsrv/elfldr.c as a separate controller source.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#endif
#include "elfldr.c"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#ifndef DF_1_PIE
#define DF_1_PIE 0x08000000
#endif

static bool g_trace_continued = false;
static bool g_trace_stopped = false;
static unsigned g_import_resolved = 0;
static unsigned g_import_unresolved = 0;
static char g_first_unresolved[128];

static const char* const g_resolve_libraries[] = {
    "libkernel_sys.sprx",
    "libkernel.sprx",
    "libSceLibcInternal.sprx",
    "libmonosgen-2.0.sprx",
    "libmonosgen-2.0.0.sprx",
    "libSceNet.sprx",
    "libSceSystemService.sprx",
    "libSceSysmodule.sprx",
    "libSceUserService.sprx",
};

static int commonfps_trace_continue(pid_t pid) {
    if (pt_continue(pid, SIGCONT) != 0)
        return -1;
    g_trace_continued = true;
    return 0;
}

static int commonfps_trace_stop(pid_t pid) {
    if (!g_trace_continued || g_trace_stopped)
        return 0;
    if (kill(pid, SIGSTOP) != 0)
        return -1;
    if (waitpid(pid, 0, 0) < 0)
        return -1;
    g_trace_stopped = true;
    return 0;
}

static intptr_t commonfps_resolve_external(pid_t pid, const char* name) {
    if (!name || !*name)
        return 0;

    const size_t count =
        sizeof(g_resolve_libraries) / sizeof(g_resolve_libraries[0]);

    for (size_t i = 0; i < count; ++i) {
        uint32_t handle = 0;
        if (kernel_dynlib_handle(pid, g_resolve_libraries[i], &handle) < 0 ||
            handle == 0) {
            continue;
        }

        const intptr_t address =
            kernel_dynlib_dlsym(pid, handle, name);
        if (address != 0)
            return address;
    }

    return 0;
}

static int commonfps_apply_relocations(
    elfldr_ctx_t* ctx,
    Elf64_Ehdr* ehdr,
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
            if ((symsec->sh_type == SHT_DYNSYM ||
                 symsec->sh_type == SHT_SYMTAB) &&
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
            const uint32_t sym_index =
                (uint32_t)ELF64_R_SYM(rela[j].r_info);
            intptr_t* location =
                (intptr_t*)((uint8_t*)ctx->base_mirror + rela[j].r_offset);

            if (type == R_X86_64_RELATIVE) {
                *location = ctx->base_addr + rela[j].r_addend;
                continue;
            }

            if (type == R_X86_64_NONE)
                continue;

            if (type != R_X86_64_64 && type != R_X86_64_GLOB_DAT)
                return -1;

            if (!dynsym || !dynstr || sym_index >= dynsym_count)
                return -1;

            const Elf64_Sym* symbol = &dynsym[sym_index];
            if (symbol->st_name >= dynstr_size)
                return -1;

            const char* name = dynstr + symbol->st_name;
            intptr_t resolved = 0;

            if (symbol->st_shndx == SHN_ABS) {
                resolved = symbol->st_value;
            } else if (symbol->st_shndx != SHN_UNDEF) {
                resolved = ctx->base_addr + symbol->st_value;
            } else {
                resolved = commonfps_resolve_external(ctx->pid, name);
                if (resolved == 0) {
                    ++g_import_unresolved;
                    if (g_first_unresolved[0] == '\0') {
                        strncpy(
                            g_first_unresolved,
                            name,
                            sizeof(g_first_unresolved) - 1);
                        g_first_unresolved[
                            sizeof(g_first_unresolved) - 1] = '\0';
                    }
                    return -1;
                }
                ++g_import_resolved;
            }

            *location = resolved + rela[j].r_addend;
        }
    }

    return 0;
}

/*
 * This loader is intentionally narrower than shsrv's generic payload loader.
 * The embedded renderer must be a true shared object whose exported elf_main
 * lies in an executable PT_LOAD.  In particular, a PIE payload would enter
 * through its private _start/CRT path and leave a second payload runtime
 * resident inside SceShellUI -- the last major structural mismatch with the
 * stable v1.0.0 overlay.
 */
static int commonfps_validate_shared_renderer(
    const uint8_t* elf,
    const Elf64_Ehdr* ehdr,
    const Elf64_Phdr* phdr) {

    if (ehdr->e_type != ET_DYN || ehdr->e_machine != EM_X86_64 ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr) || ehdr->e_phnum == 0) {
        return -1;
    }

    bool entry_is_executable = false;
    bool have_load = false;

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const Elf64_Phdr* segment = &phdr[i];

        if (segment->p_type == PT_INTERP)
            return -1;

        if (segment->p_type == PT_LOAD) {
            have_load = true;
            if ((segment->p_flags & PF_X) != 0 &&
                ehdr->e_entry >= segment->p_vaddr &&
                ehdr->e_entry - segment->p_vaddr < segment->p_memsz) {
                entry_is_executable = true;
            }
            continue;
        }

        if (segment->p_type != PT_DYNAMIC)
            continue;

        const Elf64_Dyn* dynamic =
            (const Elf64_Dyn*)(elf + segment->p_offset);
        const size_t count = segment->p_filesz / sizeof(Elf64_Dyn);
        for (size_t j = 0; j < count; ++j) {
            if (dynamic[j].d_tag == DT_NULL)
                break;
            if (dynamic[j].d_tag == DT_FLAGS_1 &&
                (dynamic[j].d_un.d_val & DF_1_PIE) != 0) {
                return -1;
            }
        }
    }

    return have_load && entry_is_executable ? 0 : -1;
}

intptr_t commonfps_stable_elfldr_load(pid_t pid, uint8_t* elf) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf;
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf + ehdr->e_phoff);
    Elf64_Shdr* shdr = (Elf64_Shdr*)(elf + ehdr->e_shoff);
    elfldr_ctx_t ctx = {.elf = elf, .pid = pid};

    g_trace_continued = false;
    g_trace_stopped = false;
    g_import_resolved = 0;
    g_import_unresolved = 0;
    memset(g_first_unresolved, 0, sizeof(g_first_unresolved));

    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F' ||
        ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
        commonfps_validate_shared_renderer(elf, ehdr, phdr) != 0) {
        return 0;
    }

    size_t min_vaddr = (size_t)-1;
    size_t max_vaddr = 0;
    int error = 0;

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD)
            continue;
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
    ctx.base_addr = 0;

    ctx.base_addr = pt_mmap(
        pid,
        ctx.base_addr,
        ctx.base_size,
        prot,
        flags,
        -1,
        0);
    if (ctx.base_addr == -1)
        return 0;

    if (commonfps_trace_continue(pid) != 0)
        error = -1;

    ctx.base_mirror = mmap(
        0,
        ctx.base_size,
        prot,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);
    if (ctx.base_mirror == MAP_FAILED)
        error = -1;

    for (int i = 0; i < ehdr->e_phnum && !error; ++i) {
        if (phdr[i].p_type == PT_LOAD)
            error = pt_load(&ctx, &phdr[i]);
    }

    if (!error)
        error = commonfps_apply_relocations(&ctx, ehdr, shdr);

    if (commonfps_trace_stop(pid) != 0)
        error = -1;

    if (!error &&
        pt_copyin(ctx.pid, ctx.base_mirror, ctx.base_addr, ctx.base_size) != 0) {
        error = -1;
    }

    for (int i = 0; i < ehdr->e_phnum && !error; ++i) {
        if (phdr[i].p_type != PT_LOAD || phdr[i].p_memsz == 0)
            continue;

        const intptr_t address = ctx.base_addr + phdr[i].p_vaddr;
        const size_t size = ROUND_PG(phdr[i].p_memsz);
        const int segment_prot = PFLAGS(phdr[i].p_flags);

        if (phdr[i].p_flags & PF_X) {
            if (kernel_mprotect(pid, address, size, segment_prot) != 0)
                error = -1;
        } else if (pt_mprotect(pid, address, size, segment_prot) != 0) {
            error = -1;
        }
    }

    if (!error && pt_msync(pid, ctx.base_addr, ctx.base_size, MS_SYNC) != 0)
        error = -1;

    if (ctx.base_mirror != MAP_FAILED)
        munmap(ctx.base_mirror, ctx.base_size);

    if (error) {
        if (g_trace_continued && !g_trace_stopped)
            (void)commonfps_trace_stop(pid);
        (void)pt_munmap(pid, ctx.base_addr, ctx.base_size);
        return 0;
    }

    return ctx.base_addr + ehdr->e_entry;
}

intptr_t commonfps_stable_elfldr_payload_args(pid_t pid) {
    return elfldr_payload_args(pid);
}

int commonfps_stable_trace_continue_seen(void) {
    return g_trace_continued ? 1 : 0;
}

int commonfps_stable_trace_stop_seen(void) {
    return g_trace_stopped ? 1 : 0;
}

unsigned commonfps_stable_import_resolved_count(void) {
    return g_import_resolved;
}

unsigned commonfps_stable_import_unresolved_count(void) {
    return g_import_unresolved;
}

const char* commonfps_stable_first_unresolved(void) {
    return g_first_unresolved;
}
