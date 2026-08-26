/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/constants.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/lifecycle.hpp"
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

    assert(!f.loading);
    assert(f.fps == 60);
}

int main() {
    test_config();
    test_layout_all_corners();
    test_exact_counter_algorithm_and_lifecycle();
    test_wire_roundtrip();
    test_integer_only_fps();

    std::cout << "Common FPS alpha2 core tests: PASS\n";
    return 0;
}
