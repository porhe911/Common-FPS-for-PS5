/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/constants.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/lifecycle.hpp"
#include "common_fps/wire.hpp"

#include <cassert>
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

    std::optional<ProcessId> find_game_process() override { return active_pid; }
    bool process_alive(ProcessId pid) override {
        return alive && active_pid && *active_pid == pid;
    }
    std::optional<ModuleInfo> find_module(ProcessId pid, const char* name) override {
        if (!process_alive(pid)) return std::nullopt;
        return ModuleInfo{module_base, name};
    }
    bool read_memory(ProcessId pid, std::uintptr_t address, void* out, std::size_t size) override {
        if (!process_alive(pid)) return false;
        const auto table_address = module_base + kVideoOutProbeTableOffset;
        if (address == table_address) {
            const std::size_t expected = kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;
            assert(size == expected);
            std::vector<std::uint8_t> table(expected, 0);
            const std::uint32_t enabled = 1;
            std::memcpy(table.data(), &enabled, sizeof(enabled));
            std::memcpy(table.data() + 0x08, &record_pointer, sizeof(record_pointer));
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
    std::uint64_t monotonic_us() override { return now_us; }
    void sleep_ms(unsigned ms) override { now_us += static_cast<std::uint64_t>(ms) * 1000ULL; }
    void advance(unsigned frames, unsigned ms) {
        counter += frames;
        now_us += static_cast<std::uint64_t>(ms) * 1000ULL;
    }
};

static void test_config() {
    auto cfg = parse_config_text("[overlay]\ncorner=top_right\nfont_size=31\nmargin_x=15\nmargin_y=20\n");
    assert(cfg.corner == Corner::TopRight);
    assert(cfg.font_size == 31);
    auto bad = parse_config_text("font_size=999\ncorner=garbage\n");
    assert(bad.font_size == kMaxFontSize);
    assert(bad.corner == Corner::BottomLeft);
}

static void test_layout_all_corners() {
    OverlayConfig cfg;
    cfg.font_size = 26;
    cfg.corner = Corner::TopLeft; auto a = compute_anchor(cfg); assert(a.x == 10.0f && a.y == 10.0f);
    cfg.corner = Corner::TopRight; a = compute_anchor(cfg); assert(a.x > 1700.0f && a.y == 10.0f);
    cfg.corner = Corner::BottomLeft; a = compute_anchor(cfg); assert(a.x == 10.0f && a.y > 1000.0f);
    cfg.corner = Corner::BottomRight; a = compute_anchor(cfg); assert(a.x > 1700.0f && a.y > 1000.0f);
}

static void warm_up(Lifecycle& life, MockPlatform& p, unsigned nominal_frames) {
    auto f = life.tick();
    assert(f.loading); // baseline
    p.advance(nominal_frames, 1000);
    f = life.tick();
    assert(f.loading); // v11: discard first delta
}

static void test_exact_counter_algorithm_and_lifecycle() {
    MockPlatform p;
    Lifecycle life(p, OverlayConfig{});
    auto f = life.tick();
    assert(f.loading);

    p.active_pid = 100; p.alive = true;
    warm_up(life, p, 60);

    p.advance(60, 1000);
    f = life.tick(); assert(!f.loading && f.fps == 60);
    const auto anchor = f.anchor;

    p.advance(59, 1000);
    f = life.tick();
    assert(!f.loading && f.fps == 59);
    assert(f.anchor.x == anchor.x && f.anchor.y == anchor.y);

    p.alive = false;
    f = life.tick(); assert(f.loading);

    p.active_pid = 200; p.alive = true; p.counter = 5000;
    warm_up(life, p, 30);
    p.advance(30, 1000);
    f = life.tick(); assert(!f.loading && f.fps == 30);
}

static void test_integer_only_and_transient_filter() {
    MockPlatform p;
    Lifecycle life(p, OverlayConfig{});
    p.active_pid = 300; p.alive = true; p.counter = 10000;
    warm_up(life, p, 60);

    p.advance(596, 10000); // 59.6 -> integer 60
    auto f = life.tick();
    assert(!f.loading && f.fps == 60);

    p.advance(200, 1000); // 200 FPS startup/junk transient: reject
    f = life.tick();
    assert(f.loading);

    p.advance(60, 1000); // state remains usable immediately after rejected delta
    f = life.tick();
    assert(!f.loading && f.fps == 60);
}

static void test_wire_roundtrip() {
    OverlayFrame frame;
    frame.visible = true; frame.loading = false; frame.fps = 60;
    frame.config.corner = Corner::BottomRight; frame.config.font_size = 26;
    frame.config.margin_x = 12.0f; frame.config.margin_y = 14.0f;
    frame.anchor = compute_anchor(frame.config);
    const auto decoded = decode_wire_packet(make_wire_packet(frame, 1234));
    assert(decoded && decoded->fps == 60 && !decoded->loading);
    assert(decoded->config.corner == Corner::BottomRight);
    assert(decoded->config.font_size == 26);
}

int main() {
    test_config();
    test_layout_all_corners();
    test_exact_counter_algorithm_and_lifecycle();
    test_integer_only_and_transient_filter();
    test_wire_roundtrip();
    std::cout << "Common FPS v11 core tests: PASS\n";
    return 0;
}
