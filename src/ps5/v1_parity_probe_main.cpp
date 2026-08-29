/*
 * Common FPS for PS5
 * Hardware parity probe v2 for the reconstructed stable v1.0.0 FPS sampler.
 *
 * This diagnostic ELF intentionally does not install the ShellUI renderer and
 * does not replace the shipping controller/plugin. It writes every completed
 * stage to /data/CommonFPS_v1_probe.log and flushes immediately, so a silent
 * notification path cannot hide where FW 9.60 parity stops.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/constants.hpp"
#include "common_fps/v1_stable_sampler.hpp"
#include "v1_stable_ps5_platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {

typedef struct notify_request {
    char padding[45];
    char message[3075];
} notify_request_t;

int sceKernelSendNotificationRequest(
    int device,
    notify_request_t* request,
    std::size_t request_size,
    int flags);
}

namespace {

constexpr const char* kLogPath = "/data/CommonFPS_v1_probe.log";

FILE* g_log = nullptr;

void log_line(const char* fmt, ...) {
    if (!g_log)
        return;

    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_log, fmt, args);
    va_end(args);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

void notify(const char* message) {
    notify_request_t request{};
    std::snprintf(request.message, sizeof(request.message), "%s", message);
    const int rc = sceKernelSendNotificationRequest(
        0, &request, sizeof(request), 0);
    log_line("NOTIFY rc=%d text=%s", rc, message);
}

} // namespace

int main() {
    g_log = std::fopen(kLogPath, "w");
    if (g_log) {
        std::setvbuf(g_log, nullptr, _IONBF, 0);
        log_line("S0 main entered");
    }

    notify("Common FPS parity probe v2\nSTART");

    common_fps::ps5::V1StablePs5Platform platform;

    std::optional<common_fps::ProcessId> pid;
    for (unsigned attempt = 0; attempt < 30; ++attempt) {
        pid = platform.find_game_process();
        if (pid)
            break;
        if (attempt == 0)
            log_line("S1 waiting for eboot.bin");
        platform.sleep_ms(1000);
    }

    if (!pid) {
        log_line("FAIL S1 game process not found after 30s");
        notify("Common FPS parity probe v2\nFAIL S1: game not found");
        if (g_log)
            std::fclose(g_log);
        return 11;
    }
    log_line("S1 game pid=%d", *pid);

    const auto module = platform.find_module(*pid, "libSceVideoOut.sprx");
    if (!module || module->base == 0) {
        log_line("FAIL S2 libSceVideoOut.sprx not found");
        notify("Common FPS parity probe v2\nFAIL S2: VideoOut module");
        if (g_log)
            std::fclose(g_log);
        return 12;
    }
    log_line(
        "S2 VideoOut base=0x%llx",
        static_cast<unsigned long long>(module->base));

    constexpr std::size_t kTableSize =
        common_fps::kVideoOutProbeEntryCount *
        common_fps::kVideoOutProbeEntrySize;
    std::array<std::uint8_t, kTableSize> table{};

    const auto table_address =
        module->base + common_fps::kVideoOutProbeTableOffset;
    const bool table_ok = platform.read_memory(
        *pid, table_address, table.data(), table.size());

    const auto& dmap = platform.dmap_backend();
    log_line(
        "S3 DMAP table_read=%s sdk=0x%08x dmap=0x%llx addr=0x%llx size=0x%llx",
        table_ok ? "OK" : "FAIL",
        dmap.last_sdk_version(),
        static_cast<unsigned long long>(dmap.last_dmap_base()),
        static_cast<unsigned long long>(table_address),
        static_cast<unsigned long long>(table.size()));

    if (!table_ok) {
        notify("Common FPS parity probe v2\nFAIL S3: DMAP table read");
        if (g_log)
            std::fclose(g_log);
        return 13;
    }

    common_fps::v1_stable::Sampler sampler(platform);
    if (!sampler.attach(*pid)) {
        log_line(
            "FAIL S4 sampler attach sdk=0x%08x dmap=0x%llx",
            dmap.last_sdk_version(),
            static_cast<unsigned long long>(dmap.last_dmap_base()));
        notify("Common FPS parity probe v2\nFAIL S4: sampler attach");
        if (g_log)
            std::fclose(g_log);
        return 14;
    }

    log_line(
        "S4 sampler attached counter=0x%llx",
        static_cast<unsigned long long>(sampler.counter_address()));

    for (unsigned attempt = 0; attempt < 15; ++attempt) {
        const auto fps = sampler.sample();
        if (fps) {
            log_line("S5 FPS OK value=%.3f", *fps);
            char message[192]{};
            std::snprintf(
                message,
                sizeof(message),
                "Common FPS parity probe v2\nDMAP FW 9.60 OK: %.1f FPS",
                *fps);
            notify(message);
            platform.sleep_ms(1500);
            if (g_log)
                std::fclose(g_log);
            return 0;
        }

        log_line(
            "S5 sample %u no-value attached=%s",
            attempt + 1,
            sampler.attached() ? "yes" : "no");

        if (!sampler.attached())
            break;
        platform.sleep_ms(1000);
    }

    log_line("FAIL S5 no valid FPS");
    notify("Common FPS parity probe v2\nFAIL S5: no valid FPS");
    if (g_log)
        std::fclose(g_log);
    return 15;
}
