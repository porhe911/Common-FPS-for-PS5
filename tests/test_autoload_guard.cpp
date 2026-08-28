/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/autoload_guard.hpp"
#include "common_fps/scene_guard.hpp"
#include "common_fps/renderer_health.hpp"

#include <cassert>
#include <iostream>

using namespace common_fps;

class MockAutoload final : public AutoloadBackend {
public:
    bool runtime = false;
    bool renderer = false;
    unsigned installs = 0;
    unsigned fail_installs = 0;

    bool runtime_ready() override {
        return runtime;
    }

    bool renderer_alive() override {
        return renderer;
    }

    bool install_renderer_once() override {
        ++installs;

        if (fail_installs) {
            --fail_installs;
            renderer = false;
            return false;
        }

        renderer = true;
        return true;
    }
};

class MockScene final : public SceneBackend {
public:
    bool scene = false;
    bool widgets = false;
    unsigned creates = 0;
    unsigned fail_creates = 0;

    bool scene_ready() override {
        return scene;
    }

    bool widgets_alive() override {
        return widgets;
    }

    bool create_widgets_once() override {
        ++creates;

        if (fail_creates) {
            --fail_creates;
            widgets = false;
            return false;
        }

        widgets = true;
        return true;
    }
};

static void test_autoload_does_not_install_too_early() {
    MockAutoload b;
    AutoloadPolicy p;
    p.consecutive_ready_samples = 3;
    p.retry_delay_us = 1'500'000ULL;

    AutoloadGuard g(b, p);

    std::uint64_t now = 0;

    // etaHEN/ShellUI not ready: absolutely no injection.
    for (int i = 0; i < 20; ++i) {
        g.tick(now);
        now += 250'000ULL;
    }

    assert(b.installs == 0);
    assert(!g.ready());

    // One positive readiness sample is still not enough.
    b.runtime = true;
    g.tick(now);
    now += 250'000ULL;
    assert(b.installs == 0);

    // Runtime flickers back to false -> stabilization counter resets.
    b.runtime = false;
    g.tick(now);
    now += 250'000ULL;
    assert(b.installs == 0);

    b.runtime = true;

    g.tick(now); now += 250'000ULL; // 1
    g.tick(now); now += 250'000ULL; // 2
    assert(b.installs == 0);

    g.tick(now); // 3 -> install
    assert(b.installs == 1);
    assert(g.ready());
    assert(b.renderer);
}

static void test_failed_early_install_is_not_terminal() {
    MockAutoload b;
    b.runtime = true;
    b.fail_installs = 1;

    AutoloadPolicy p;
    p.consecutive_ready_samples = 3;
    p.retry_delay_us = 1'500'000ULL;

    AutoloadGuard g(b, p);

    std::uint64_t now = 0;

    g.tick(now); now += 250'000ULL;
    g.tick(now); now += 250'000ULL;
    g.tick(now); // first install fails

    assert(b.installs == 1);
    assert(!g.ready());
    assert(g.state() == AutoloadState::WaitingToRetry);

    // The worker remains alive and does not hammer the loader.
    now += 500'000ULL;
    g.tick(now);
    assert(b.installs == 1);

    // After retry delay, require stable runtime again.
    now = 2'000'000ULL;
    g.tick(now); now += 250'000ULL;
    g.tick(now); now += 250'000ULL;
    g.tick(now);

    assert(b.installs == 2);
    assert(g.ready());
    assert(b.renderer);
}

static void test_false_enabled_state_self_heals() {
    MockAutoload b;
    b.runtime = true;

    AutoloadPolicy p;
    p.consecutive_ready_samples = 2;
    p.retry_delay_us = 1'000'000ULL;

    AutoloadGuard g(b, p);

    std::uint64_t now = 0;
    g.tick(now);
    now += 250'000ULL;
    g.tick(now);

    assert(g.ready());
    assert(b.installs == 1);

    // Simulate renderer worker disappearing while etaHEN still thinks the
    // plugin is enabled.
    b.renderer = false;

    now += 250'000ULL;
    g.tick(now);

    assert(!g.ready());
    assert(g.state() == AutoloadState::WaitingToRetry);

    now += 1'000'000ULL;
    g.tick(now);
    now += 250'000ULL;
    g.tick(now);

    assert(g.ready());
    assert(b.installs == 2);
}


static void test_renderer_health_protocol() {
    RendererHealthPacket packet{};

    assert(valid_renderer_health_packet(packet));
    assert(packet.size == kRendererHealthPacketSize);

    packet.phase =
        static_cast<std::uint16_t>(RendererHealthPhase::VisualReady);
    assert(valid_renderer_health_packet(packet));

    packet.magic = 0;
    assert(!valid_renderer_health_packet(packet));
}

static void test_scene_one_shot_failure_retries() {
    MockScene b;
    ScenePolicy p;
    p.consecutive_ready_samples = 2;

    SceneGuard g(b, p);

    // Payload can exist early, but it must not create widgets yet.
    for (int i = 0; i < 10; ++i)
        g.tick();

    assert(b.creates == 0);
    assert(!g.ready());

    b.scene = true;
    b.fail_creates = 1;

    g.tick(); // stabilize 1
    g.tick(); // create attempt fails

    assert(b.creates == 1);
    assert(!g.ready());

    // Unlike v1.0.0 autoload failure, the renderer code stays alive.
    g.tick();
    g.tick();

    assert(b.creates == 2);
    assert(g.ready());

    // Scene replacement should also self-heal.
    b.widgets = false;
    b.scene = false;
    g.tick();
    assert(!g.ready());

    b.scene = true;
    g.tick();
    g.tick();

    assert(g.ready());
    assert(b.creates == 3);
}

int main() {
    test_autoload_does_not_install_too_early();
    test_failed_early_install_is_not_terminal();
    test_false_enabled_state_self_heals();
    test_renderer_health_protocol();
    test_scene_one_shot_failure_retries();

    std::cout << "Common FPS safe autoload tests: PASS\n";
    return 0;
}
