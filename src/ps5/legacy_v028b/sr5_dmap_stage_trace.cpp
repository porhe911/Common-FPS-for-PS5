/*
 * Common FPS v0.28b stable-source rebuild - SR5 DMAP stage trace
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Keeps the hardware-stable SR4 lifecycle/module discovery.  After VideoOut is
 * found and Auth is restored, this probe traces the exact FW 9.60 translation
 * chain recovered from the reference v0.28b ELF.  If translation succeeds, the
 * same local reference reader is used for VideoOut table/root/counter reads.
 */

#include "process_sysctl.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_SR5_dmap_stage_trace.log";
constexpr int kRuntimeSeconds = 300;
constexpr int kDiscoveryDelaySeconds = 4;
constexpr int kDiscoveryRetrySeconds = 5;
constexpr std::uint32_t kMaxTenthsFps = 3000U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;

constexpr std::uint64_t kDebuggerAuthId = 0x4800000000000006ULL;
constexpr std::uintptr_t kAuthIdOffset = 0x58ULL;
constexpr long kSysDlGetList = 0x217;
constexpr long kSysDlGetInfo2 = 0x2cd;
constexpr long kExpectedSizeQueryRc = 12;

constexpr char kVideoOutModule[] = "libSceVideoOut.sprx";
constexpr std::uintptr_t kProbeTableOffset = 0x34980ULL;
constexpr std::size_t kProbeTableSize = 0xA8ULL;
constexpr std::size_t kProbeEntrySize = 0x18ULL;
constexpr std::size_t kProbeEntryCount = kProbeTableSize / kProbeEntrySize;
constexpr std::uintptr_t kCounterOffset = 0x768ULL;

// Exact FW 9.60 fields recovered from the hardware-proven v0.28b firmware table.
constexpr std::uint32_t kFwMin = 0x09600000U;
constexpr std::uint32_t kFwMax = 0x0960FFFFU;
constexpr std::uintptr_t kAllprocFromKdata = 0x02755D50ULL;
constexpr std::uintptr_t kProcPid = 0xBCULL;
constexpr std::uintptr_t kProcVmspace = 0x200ULL;
constexpr std::uintptr_t kProcNext = 0x00ULL;
constexpr std::uintptr_t kVmspacePmap = 0x1D8ULL;
constexpr std::uintptr_t kPmapPml4Virtual = 0x20ULL;
constexpr std::uintptr_t kPmapPml4Physical = 0x28ULL;
constexpr std::uintptr_t kFixedDmapBase = 0xFFFF873B00000000ULL;

constexpr std::uint64_t kPresent = 1ULL;
constexpr std::uint64_t kPageSizeBit = 1ULL << 7;
constexpr std::uint64_t kMask4K = 0x000FFFFFFFFFF000ULL;
constexpr std::uint64_t kMask2M = 0x000FFFFFFFE00000ULL;
constexpr std::uint64_t kMask1G = 0x000FFFFFC0000000ULL;
constexpr std::size_t kPage4K = 0x1000ULL;
constexpr std::size_t kPage2M = 0x200000ULL;
constexpr std::size_t kPage1G = 0x40000000ULL;

constexpr std::size_t kModuleNameLength = 128;
constexpr std::size_t kSandboxPathLength = 1024;
constexpr std::size_t kMaxSections = 4;
constexpr std::size_t kFingerprintLength = 20;

struct ModuleSectionCompat {
    std::uint64_t vaddr;
    std::uint32_t size;
    std::uint32_t prot;
};

struct ModuleInfoCompat {
    char filename[kModuleNameLength];
    std::uint64_t handle;
    std::uint8_t unknown0[32];
    std::uint64_t init;
    std::uint64_t fini;
    std::uint64_t eh_frame_hdr;
    std::uint64_t eh_frame_hdr_sz;
    std::uint64_t eh_frame;
    std::uint64_t eh_frame_sz;
    ModuleSectionCompat sections[kMaxSections];
    std::uint8_t unknown7[1176];
    std::uint8_t fingerprint[kFingerprintLength];
    std::uint32_t unknown8;
    char libname[kModuleNameLength];
    std::uint32_t unknown9;
    char sandboxed_path[kSandboxPathLength];
    std::uint64_t sdk_version;
};

struct Translation {
    std::uintptr_t physical = 0;
    std::size_t page_size = 0;
    std::uintptr_t dmap = 0;
};

void log_line(const char* text) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

void log_pid(const char* prefix, pid_t pid) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "%s%d\n", prefix, static_cast<int>(pid));
    std::fclose(f);
}

void log_auth(const char* tag, std::uint64_t value) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR5 %s auth=0x%016llx\n", tag,
                 static_cast<unsigned long long>(value));
    std::fclose(f);
}

void log_list(const char* stage, long rc, std::size_t count) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR5 MODULE %s rc=%ld count=%zu\n", stage, rc, count);
    std::fclose(f);
}

void log_u64(const char* stage, std::uintptr_t value) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR5 TRACE %s=0x%016llx\n", stage,
                 static_cast<unsigned long long>(value));
    std::fclose(f);
}

void log_read(const char* stage, int rc, std::uintptr_t address, std::uintptr_t value) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR5 TRACE %s rc=%d address=0x%016llx value=0x%016llx\n",
                 stage, rc,
                 static_cast<unsigned long long>(address),
                 static_cast<unsigned long long>(value));
    std::fclose(f);
}

void log_fps(pid_t pid, std::uint32_t tenths) noexcept {
    FILE* f = std::fopen(kLogPath, "a");
    if (!f) return;
    std::fprintf(f, "SR5 FPS pid=%d value=%u.%u\n",
                 static_cast<int>(pid), tenths / 10U, tenths % 10U);
    std::fclose(f);
}

std::int64_t elapsed_us(const timeval& before, const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) * 1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

template <typename T>
bool kread(std::uintptr_t address, T& value, const char* stage, bool trace) noexcept {
    value = {};
    const int rc = kernel_copyout(address, &value, sizeof(value));
    if (trace)
        log_read(stage, rc, address, static_cast<std::uintptr_t>(value));
    return rc == 0;
}

std::optional<std::uintptr_t> find_videoout_base(pid_t game_pid) noexcept {
    const std::uintptr_t self_ucred = kernel_get_proc_ucred(getpid());
    if (self_ucred == 0) {
        log_line("SR5 AUTH failed: kernel_get_proc_ucred(self)=0");
        return std::nullopt;
    }

    std::uint64_t saved_auth = 0;
    if (kernel_copyout(self_ucred + kAuthIdOffset, &saved_auth, sizeof(saved_auth)) != 0) {
        log_line("SR5 AUTH failed: read current Auth ID");
        return std::nullopt;
    }
    log_auth("AUTH saved", saved_auth);

    const std::uint64_t debug_auth = kDebuggerAuthId;
    if (kernel_copyin(&debug_auth, self_ucred + kAuthIdOffset, sizeof(debug_auth)) != 0) {
        log_line("SR5 AUTH failed: set debugger Auth ID");
        return std::nullopt;
    }
    log_line("SR5 AUTH debugger active for module discovery only");

    std::optional<std::uintptr_t> result;
    std::size_t count = 0;
    const long rc_size = syscall(kSysDlGetList, game_pid, nullptr, 0, &count);
    log_list("list-size", rc_size, count);

    if ((rc_size == 0 || rc_size == kExpectedSizeQueryRc) && count > 0 && count < 1024) {
        if (rc_size == kExpectedSizeQueryRc)
            log_line("SR5 MODULE size-query rc=12 accepted");
        auto* handles = static_cast<std::uintptr_t*>(std::calloc(count, sizeof(std::uintptr_t)));
        if (handles) {
            std::size_t returned = count;
            const long rc_fill = syscall(kSysDlGetList, game_pid, handles, count, &returned);
            log_list("list-fill", rc_fill, returned);
            if (rc_fill == 0 && returned <= count) {
                for (std::size_t i = 0; i < returned; ++i) {
                    ModuleInfoCompat info{};
                    const long rc_info = syscall(kSysDlGetInfo2, game_pid, 1, handles[i], &info);
                    if (rc_info != 0) continue;
                    info.filename[kModuleNameLength - 1] = '\0';
                    if (std::strcmp(info.filename, kVideoOutModule) == 0) {
                        FILE* f = std::fopen(kLogPath, "a");
                        if (f) {
                            std::fprintf(f,
                                "SR5 MODULE FOUND index=%zu name=%s handle=0x%llx base=0x%llx\n",
                                i, info.filename,
                                static_cast<unsigned long long>(info.handle),
                                static_cast<unsigned long long>(info.sections[0].vaddr));
                            std::fclose(f);
                        }
                        if (info.sections[0].vaddr != 0)
                            result = static_cast<std::uintptr_t>(info.sections[0].vaddr);
                        break;
                    }
                }
            }
            std::free(handles);
        }
    }

    if (kernel_copyin(&saved_auth, self_ucred + kAuthIdOffset, sizeof(saved_auth)) != 0) {
        log_line("SR5 AUTH RESTORE FAILED");
        return std::nullopt;
    }
    log_line("SR5 AUTH restored before all DMAP work");
    return result;
}

std::optional<Translation> translate_reference(pid_t pid, std::uintptr_t va, bool trace) noexcept {
    std::uint32_t sdk = 0;
    std::size_t sdk_len = sizeof(sdk);
    const int sdk_rc = sysctlbyname("kern.sdk_version", &sdk, &sdk_len, nullptr, 0);
    if (trace) {
        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "SR5 TRACE sdk sysctl rc=%d value=0x%08x len=%zu\n", sdk_rc, sdk, sdk_len);
            std::fclose(f);
        }
        const std::uint32_t crt_fw = kernel_get_fw_version();
        FILE* f2 = std::fopen(kLogPath, "a");
        if (f2) {
            std::fprintf(f, "SR5 TRACE kernel_get_fw_version=0x%08x\n", crt_fw);
            std::fclose(f2);
        }
        log_u64("KERNEL_ADDRESS_DATA_BASE", static_cast<std::uintptr_t>(KERNEL_ADDRESS_DATA_BASE));
        log_u64("KERNEL_ADDRESS_ALLPROC", static_cast<std::uintptr_t>(KERNEL_ADDRESS_ALLPROC));
    }
    if (sdk_rc != 0 || sdk < kFwMin || sdk > kFwMax) {
        log_line("SR5 TRACE FAIL unsupported sdk from kern.sdk_version");
        return std::nullopt;
    }

    const std::uintptr_t allproc_addr =
        static_cast<std::uintptr_t>(KERNEL_ADDRESS_DATA_BASE) + kAllprocFromKdata;
    if (trace) log_u64("expected_allproc_address", allproc_addr);

    std::uintptr_t current = 0;
    if (!kread(allproc_addr, current, "allproc_head", trace) || current == 0) {
        log_line("SR5 TRACE FAIL allproc head");
        return std::nullopt;
    }

    std::uintptr_t proc = 0;
    unsigned walked = 0;
    for (; current != 0 && walked < 1024; ++walked) {
        std::uint32_t candidate_pid = 0;
        if (kernel_copyout(current + kProcPid, &candidate_pid, sizeof(candidate_pid)) != 0) {
            log_line("SR5 TRACE FAIL proc pid read");
            return std::nullopt;
        }
        if (candidate_pid == static_cast<std::uint32_t>(pid)) {
            proc = current;
            break;
        }
        std::uintptr_t next = 0;
        if (kernel_copyout(current + kProcNext, &next, sizeof(next)) != 0 || next == current) {
            log_line("SR5 TRACE FAIL proc next read");
            return std::nullopt;
        }
        current = next;
    }
    if (trace) {
        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "SR5 TRACE proc=0x%016llx walked=%u target_pid=%d\n",
                         static_cast<unsigned long long>(proc), walked, static_cast<int>(pid));
            std::fclose(f);
        }
    }
    if (proc == 0) {
        log_line("SR5 TRACE FAIL proc not found");
        return std::nullopt;
    }

    std::uintptr_t vmspace = 0;
    if (!kread(proc + kProcVmspace, vmspace, "vmspace", trace) || vmspace == 0) {
        log_line("SR5 TRACE FAIL vmspace");
        return std::nullopt;
    }

    std::uintptr_t pmap = 0;
    if (!kread(vmspace + kVmspacePmap, pmap, "pmap", trace) || pmap == 0) {
        log_line("SR5 TRACE FAIL pmap");
        return std::nullopt;
    }

    std::uintptr_t pml4 = 0;
    if (!kread(pmap + kPmapPml4Virtual, pml4, "pml4_virtual", trace) || pml4 == 0) {
        log_line("SR5 TRACE FAIL pml4 virtual");
        return std::nullopt;
    }

    std::uintptr_t cr3 = 0;
    if (!kread(pmap + kPmapPml4Physical, cr3, "pml4_physical", trace) || cr3 == 0) {
        log_line("SR5 TRACE FAIL pml4 physical");
        return std::nullopt;
    }

    std::uintptr_t dmap = kFixedDmapBase;
    if (pml4 > cr3 && cr3 < (1ULL << 40)) {
        const std::uintptr_t candidate = pml4 - cr3;
        if ((candidate >> 48) != 0)
            dmap = candidate;
    }
    if (trace) log_u64("dmap", dmap);

    const std::size_t i4 = (va >> 39) & 0x1FFULL;
    const std::size_t i3 = (va >> 30) & 0x1FFULL;
    const std::size_t i2 = (va >> 21) & 0x1FFULL;
    const std::size_t i1 = (va >> 12) & 0x1FFULL;

    std::uint64_t pml4e = 0;
    if (!kread(pml4 + i4 * 8ULL, pml4e, "PML4E", trace) || (pml4e & kPresent) == 0) {
        log_line("SR5 TRACE FAIL PML4E not present");
        return std::nullopt;
    }

    std::uint64_t pdpte = 0;
    if (!kread(dmap + (pml4e & kMask4K) + i3 * 8ULL, pdpte, "PDPTE", trace) ||
        (pdpte & kPresent) == 0) {
        log_line("SR5 TRACE FAIL PDPTE not present");
        return std::nullopt;
    }
    if ((pdpte & kPageSizeBit) != 0) {
        Translation t;
        t.physical = static_cast<std::uintptr_t>(pdpte & kMask1G) | (va & (kPage1G - 1ULL));
        t.page_size = kPage1G;
        t.dmap = dmap;
        if (trace) log_u64("physical_1G", t.physical);
        return t;
    }

    std::uint64_t pde = 0;
    if (!kread(dmap + (pdpte & kMask4K) + i2 * 8ULL, pde, "PDE", trace) ||
        (pde & kPresent) == 0) {
        log_line("SR5 TRACE FAIL PDE not present");
        return std::nullopt;
    }
    if ((pde & kPageSizeBit) != 0) {
        Translation t;
        t.physical = static_cast<std::uintptr_t>(pde & kMask2M) | (va & (kPage2M - 1ULL));
        t.page_size = kPage2M;
        t.dmap = dmap;
        if (trace) log_u64("physical_2M", t.physical);
        return t;
    }

    std::uint64_t pte = 0;
    if (!kread(dmap + (pde & kMask4K) + i1 * 8ULL, pte, "PTE", trace) ||
        (pte & kPresent) == 0) {
        log_line("SR5 TRACE FAIL PTE not present");
        return std::nullopt;
    }

    Translation t;
    t.physical = static_cast<std::uintptr_t>(pte & kMask4K) | (va & (kPage4K - 1ULL));
    t.page_size = kPage4K;
    t.dmap = dmap;
    if (trace) log_u64("physical_4K", t.physical);
    return t;
}

bool proc_read_reference(pid_t pid, std::uintptr_t remote, void* local,
                         std::size_t size, bool trace_first) noexcept {
    if (size == 0) return true;
    if (pid <= 0 || remote == 0 || local == nullptr) return false;

    auto* output = static_cast<std::uint8_t*>(local);
    std::size_t remaining = size;
    bool trace = trace_first;
    while (remaining != 0) {
        const auto tr = translate_reference(pid, remote, trace);
        if (!tr || tr->physical == 0 || tr->page_size == 0) return false;

        const std::size_t page_offset = static_cast<std::size_t>(remote & (tr->page_size - 1ULL));
        const std::size_t chunk = std::min(remaining, tr->page_size - page_offset);
        const std::uintptr_t source = tr->dmap + tr->physical;
        const int rc = kernel_copyout(source, output, chunk);
        if (trace) log_read("final_DMAP_copyout", rc, source, 0);
        if (rc != 0) return false;

        remote += chunk;
        output += chunk;
        remaining -= chunk;
        trace = false;
    }
    return true;
}

std::optional<std::uintptr_t> resolve_counter(pid_t pid, std::uintptr_t module_base) noexcept {
    std::array<std::uint8_t, kProbeTableSize> table{};
    const std::uintptr_t table_addr = module_base + kProbeTableOffset;
    log_u64("VideoOut_table_va", table_addr);
    if (!proc_read_reference(pid, table_addr, table.data(), table.size(), true)) {
        log_line("SR5 DMAP table read FAILED");
        return std::nullopt;
    }
    log_line("SR5 DMAP table read OK");

    for (std::size_t i = 0; i < kProbeEntryCount; ++i) {
        const auto* entry = table.data() + i * kProbeEntrySize;
        std::uint32_t enabled = 0;
        std::uint64_t pointer = 0;
        std::memcpy(&enabled, entry + 0x00, sizeof(enabled));
        std::memcpy(&pointer, entry + 0x08, sizeof(pointer));
        if (enabled == 0 || pointer == 0) continue;

        FILE* f = std::fopen(kLogPath, "a");
        if (f) {
            std::fprintf(f, "SR5 TABLE active index=%zu enabled=%u pointer=0x%llx\n",
                         i, enabled, static_cast<unsigned long long>(pointer));
            std::fclose(f);
        }

        std::uint64_t root = 0;
        if (!proc_read_reference(pid, static_cast<std::uintptr_t>(pointer), &root, sizeof(root), true)) {
            log_line("SR5 DMAP root read FAILED");
            continue;
        }
        if (root == 0) continue;
        log_u64("VideoOut_root", static_cast<std::uintptr_t>(root));
        return static_cast<std::uintptr_t>(root) + kCounterOffset;
    }

    log_line("SR5 TABLE no usable active entry");
    return std::nullopt;
}

int run_probe() noexcept {
    using common_fps::legacy_v028b::find_game_pid_sysctl;

    log_pid("SR5 CHILD pid=", getpid());
    log_line("SR5 START exact-reference DMAP stage trace");
    log_line("SR5 NO ptrace / NO MDBG / NO renderer / NO ShellUI inject");

    pid_t active_pid = -1;
    std::uintptr_t counter_address = 0;
    std::uint32_t previous_counter = 0;
    timeval previous_time{};
    bool have_baseline = false;
    int seconds_on_pid = 0;
    int retry_countdown = 0;

    for (int second = 0; second < kRuntimeSeconds; ++second) {
        const pid_t current_pid = find_game_pid_sysctl();
        if (current_pid != active_pid) {
            active_pid = current_pid;
            counter_address = 0;
            previous_counter = 0;
            previous_time = {};
            have_baseline = false;
            seconds_on_pid = 0;
            retry_countdown = 0;
            log_pid("SR5 CHANGE eboot.bin pid=", active_pid);
        }

        if (active_pid <= 0) {
            sleep(1);
            continue;
        }
        ++seconds_on_pid;

        if (counter_address == 0) {
            if (seconds_on_pid < kDiscoveryDelaySeconds) {
                sleep(1);
                continue;
            }
            if (retry_countdown > 0) {
                --retry_countdown;
                sleep(1);
                continue;
            }

            log_line("SR5 DISCOVERY TRY stable module path + exact-reference DMAP trace");
            const auto module_base = find_videoout_base(active_pid);
            if (!module_base) {
                log_line("SR5 DISCOVERY module stage failed; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }

            const auto resolved = resolve_counter(active_pid, *module_base);
            if (!resolved) {
                log_line("SR5 DISCOVERY DMAP stage failed; retry in 5s");
                retry_countdown = kDiscoveryRetrySeconds;
                sleep(1);
                continue;
            }
            counter_address = *resolved;
            FILE* f = std::fopen(kLogPath, "a");
            if (f) {
                std::fprintf(f, "SR5 COUNTER READY pid=%d address=0x%llx\n",
                             static_cast<int>(active_pid),
                             static_cast<unsigned long long>(counter_address));
                std::fclose(f);
            }
        }

        std::uint32_t current_counter = 0;
        if (!proc_read_reference(active_pid, counter_address, &current_counter,
                                 sizeof(current_counter), false)) {
            log_line("SR5 COUNTER read failed; drop and rediscover");
            counter_address = 0;
            have_baseline = false;
            retry_countdown = kDiscoveryRetrySeconds;
            sleep(1);
            continue;
        }

        timeval now{};
        gettimeofday(&now, nullptr);
        if (!have_baseline) {
            previous_counter = current_counter;
            previous_time = now;
            have_baseline = true;
            log_line("SR5 BASELINE captured");
            sleep(1);
            continue;
        }

        const std::int64_t elapsed = elapsed_us(previous_time, now);
        if (elapsed > 0) {
            const std::uint32_t delta = static_cast<std::uint32_t>(current_counter - previous_counter);
            const std::uint64_t scaled = static_cast<std::uint64_t>(delta) * kTenthsScale;
            const std::uint32_t tenths = static_cast<std::uint32_t>(
                scaled / static_cast<std::uint64_t>(elapsed));
            if (tenths <= kMaxTenthsFps)
                log_fps(active_pid, tenths);
            else
                log_line("SR5 FPS sanity reject >300.0");
        }

        previous_counter = current_counter;
        previous_time = now;
        sleep(1);
    }

    log_line("SR5 DONE clean return after 300s");
    return 0;
}

} // namespace

extern "C" int main() {
    log_pid("SR5 PARENT start pid=", getpid());
    const pid_t child = fork();
    if (child > 0) {
        log_pid("SR5 PARENT forked child=", child);
        log_line("SR5 PARENT RETURN 0");
        return 0;
    }
    if (child < 0)
        log_line("SR5 fork failed; run in current process");
    return run_probe();
}
