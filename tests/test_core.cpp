/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/constants.hpp"
#include "common_fps/fps_sampler.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/lifecycle.hpp"
#include "common_fps/shellui_hook_protocol.hpp"
#include "common_fps/wire.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

using namespace common_fps;

class MockPlatform final : public Platform {
public:
    std::optional<ProcessId> active_pid;
    bool alive = false;

    std::uintptr_t module_base = 0x10000000;
    std::uintptr_t record_pointer = 0x20000000;
    std::uintptr_t counter_root = 0x30000000;

    std::uint32_t counter = 1000;
    std::uint64_t now_us = 1'000'000;

    std::optional<ProcessId> find_game_process() override {
        return active_pid;
    }

    bool process_alive(ProcessId pid) override {
        return alive && active_pid && *active_pid == pid;
    }

    std::optional<ModuleInfo>
    find_module(ProcessId pid, const char* name) override {
        if (!process_alive(pid))
            return std::nullopt;

        return ModuleInfo{module_base, name};
    }

    bool read_memory(
        ProcessId pid,
        std::uintptr_t address,
        void* out,
        std::size_t size) override {

        if (!process_alive(pid))
            return false;

        const auto table_address =
            module_base + kVideoOutProbeTableOffset;

        if (address == table_address) {
            const std::size_t expected =
                kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;
            assert(size == expected);

            std::vector<std::uint8_t> table(expected, 0);

            const std::uint32_t enabled = 1;
            std::memcpy(table.data() + 0x00, &enabled, sizeof(enabled));
            std::memcpy(
                table.data() + 0x08,
                &record_pointer,
                sizeof(record_pointer));

            std::memcpy(out, table.data(), expected);
            return true;
        }

        if (address == record_pointer) {
            assert(size == sizeof(counter_root));
            std::memcpy(out, &counter_root, sizeof(counter_root));
            return true;
        }

        if (address == counter_root + kVideoOutCounterOffset) {
            assert(size == sizeof(counter));
            std::memcpy(out, &counter, sizeof(counter));
            return true;
        }

        return false;
    }

    std::uint64_t monotonic_us() override {
        return now_us;
    }

    void sleep_ms(unsigned ms) override {
        now_us += static_cast<std::uint64_t>(ms) * 1000ULL;
    }

    void advance(unsigned frames, unsigned ms) {
        counter += frames;
        now_us += static_cast<std::uint64_t>(ms) * 1000ULL;
    }
};

class DynamicScanPlatform final : public Platform {
public:
    static constexpr ProcessId kPid = 760;
    static constexpr std::uintptr_t kModule = 0x40000000;
    static constexpr std::uintptr_t kRealRecordOffset = 0x1f0100;
    static constexpr std::uintptr_t kRealPointer = 0x51000000;
    static constexpr std::uintptr_t kRealRoot = 0x52000000;

    std::uint32_t counter = 2000;
    std::uint64_t now_us = 1'000'000;

    std::optional<ProcessId> find_game_process() override {
        return kPid;
    }

    bool process_alive(ProcessId pid) override {
        return pid == kPid;
    }

    std::optional<ModuleInfo>
    find_module(ProcessId pid, const char* name) override {
        if (!process_alive(pid))
            return std::nullopt;
        return ModuleInfo{kModule, name};
    }

    bool read_memory(
        ProcessId pid,
        std::uintptr_t address,
        void* out,
        std::size_t size) override {

        if (!process_alive(pid))
            return false;

        const std::uintptr_t fixed_table =
            kModule + kVideoOutProbeTableOffset;
        if (address == fixed_table) {
            std::memset(out, 0, size);
            return true;
        }

        constexpr std::uintptr_t scan_begin = 0x10000;
        constexpr std::uintptr_t scan_end = 0x200000;
        constexpr std::size_t scan_step = 0x2000;
        constexpr std::size_t scan_read_size = 0x2020;
        if (address >= kModule + scan_begin &&
            address < kModule + scan_end &&
            ((address - kModule - scan_begin) % scan_step) == 0 &&
            size == scan_read_size) {
            auto* bytes = static_cast<std::uint8_t*>(out);
            std::memset(bytes, 0, size);

            /*
             * Every window contains a plausible direct-pointer decoy. The
             * Stage 8.1 mixed queue filled before reaching the real late
             * indirect chain; Stage 8.2 must not enqueue these decoys.
             */
            const std::uint32_t enabled = 1;
            const std::uint64_t decoy =
                0x60000000ULL + (address - kModule);
            std::memcpy(bytes, &enabled, sizeof(enabled));
            std::memcpy(bytes + 0x08, &decoy, sizeof(decoy));

            const std::uintptr_t real_record =
                kModule + kRealRecordOffset;
            if (real_record >= address &&
                real_record + 0x20 <= address + size) {
                const std::size_t local =
                    static_cast<std::size_t>(real_record - address);
                std::memcpy(bytes + local, &enabled, sizeof(enabled));
                std::memcpy(
                    bytes + local + 0x08,
                    &kRealPointer,
                    sizeof(kRealPointer));
            }
            return true;
        }

        if (address == kRealPointer && size == sizeof(kRealRoot)) {
            std::memcpy(out, &kRealRoot, sizeof(kRealRoot));
            return true;
        }

        if (address == kRealRoot + kVideoOutCounterOffset &&
            size == sizeof(counter)) {
            std::memcpy(out, &counter, sizeof(counter));
            return true;
        }

        return false;
    }

    std::uint64_t monotonic_us() override {
        return now_us;
    }

    void sleep_ms(unsigned ms) override {
        counter += static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(ms) * 60ULL) / 1000ULL);
        now_us += static_cast<std::uint64_t>(ms) * 1000ULL;
    }
};

static void test_config() {
    auto cfg = parse_config_text(R"(
        [overlay]
        corner=top_right
        font_size=31
        margin_x=15
        margin_y=20
    )");

    assert(cfg.corner == Corner::TopRight);
    assert(cfg.font_size == 31);

    auto bad =
        parse_config_text("font_size=999\ncorner=garbage\n");

    assert(bad.font_size == kMaxFontSize);
    assert(bad.corner == Corner::BottomLeft);
}

static void test_layout_all_corners() {
    OverlayConfig cfg;
    cfg.font_size = 26;

    cfg.corner = Corner::TopLeft;
    auto a = compute_anchor(cfg);
    assert(a.x == 10.0f && a.y == 10.0f);

    cfg.corner = Corner::TopRight;
    a = compute_anchor(cfg);
    assert(a.x > 1700.0f && a.y == 10.0f);

    cfg.corner = Corner::BottomLeft;
    a = compute_anchor(cfg);
    assert(a.x == 10.0f && a.y > 1000.0f);

    cfg.corner = Corner::BottomRight;
    a = compute_anchor(cfg);
    assert(a.x > 1700.0f && a.y > 1000.0f);
}

static void test_exact_counter_algorithm_and_lifecycle() {
    MockPlatform p;
    OverlayConfig cfg;
    Lifecycle life(p, cfg);

    // No game.
    auto f = life.tick();
    assert(f.loading);

    // Game A appears.
    p.active_pid = 100;
    p.alive = true;

    // First reading establishes baseline.
    f = life.tick();
    assert(f.loading);

    // Hardware-proven sampler discards the first complete delta as warmup.
    p.advance(60, 1000);
    f = life.tick();
    assert(f.loading);

    // 60 frame increments during exactly one second -> 60.0 FPS.
    p.advance(60, 1000);
    f = life.tick();
    assert(!f.loading);
    assert(f.fps == 60);

    const auto anchor = f.anchor;

    // 59 frames / 1 second -> 59 FPS, without positional drift.
    p.advance(59, 1000);
    f = life.tick();
    assert(!f.loading);
    assert(f.fps == 59);
    assert(f.anchor.x == anchor.x);
    assert(f.anchor.y == anchor.y);

    // Game A exits.
    p.alive = false;
    f = life.tick();
    assert(f.loading);

    // Game B gets a new PID and a reset counter.
    p.active_pid = 200;
    p.alive = true;
    p.counter = 5000;

    f = life.tick();
    assert(f.loading); // new baseline

    p.advance(30, 1000);
    f = life.tick();
    assert(f.loading); // discarded warmup delta

    p.advance(30, 1000);
    f = life.tick();
    assert(!f.loading);
    assert(f.fps == 30);
}

static void test_wire_roundtrip() {
    OverlayFrame frame;
    frame.visible = true;
    frame.loading = false;
    frame.fps = 60;
    frame.config.corner = Corner::BottomRight;
    frame.config.font_size = 26;
    frame.config.margin_x = 12.0f;
    frame.config.margin_y = 14.0f;
    frame.anchor = compute_anchor(frame.config);

    const auto packet = make_wire_packet(frame, 1234);
    const auto decoded = decode_wire_packet(packet);

    assert(decoded);
    assert(decoded->fps == 60);
    assert(!decoded->loading);
    assert(decoded->config.corner == Corner::BottomRight);
    assert(decoded->config.font_size == 26);
}


static void test_integer_only_fps() {
    MockPlatform p;
    OverlayConfig cfg;
    Lifecycle life(p, cfg);

    p.active_pid = 300;
    p.alive = true;
    p.counter = 10000;

    auto f = life.tick();
    assert(f.loading);

    // 596 frame increments over 10 seconds = 59.6 FPS internally.
    // Public/displayed result must be integer 60.
    p.advance(596, 10000);
    f = life.tick();
    assert(f.loading); // discarded warmup delta

    p.advance(596, 10000);
    f = life.tick();

    assert(!f.loading);
    assert(f.fps == 60);
}

static void test_dynamic_scan_prioritizes_indirect_chain() {
    DynamicScanPlatform platform;
    FpsSampler sampler(platform);

    assert(sampler.attach(DynamicScanPlatform::kPid));
    assert(sampler.counter_address() ==
        DynamicScanPlatform::kRealRoot + kVideoOutCounterOffset);
}

static void test_shellui_hook_protocol_checksum() {
    ShellUiHookRequest request{};
    request.pid = 57;
    request.nonce = 0x12345678ULL;
    request.method_address = 0x100000ULL;
    request.hook_address = 0x200000ULL;
    request.trampoline_address = 0x300000ULL;
    request.displaced_size = 16;
    request.expected[0] = 0x55;
    request.desired[0] = 0xff;
    request.checksum = shellui_hook_request_checksum(request);

    assert(request.checksum == shellui_hook_request_checksum(request));
    request.expected[1] ^= 1U;
    assert(request.checksum != shellui_hook_request_checksum(request));
}

int main() {
    test_config();
    test_layout_all_corners();
    test_exact_counter_algorithm_and_lifecycle();
    test_wire_roundtrip();
    test_integer_only_fps();
    test_dynamic_scan_prioritizes_indirect_chain();
    test_shellui_hook_protocol_checksum();

    std::cout << "Common FPS alpha2 core tests: PASS\n";
    return 0;
}
