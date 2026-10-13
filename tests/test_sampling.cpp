/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/dce_counter_sampler.hpp"
#include "common_fps/fps_rate.hpp"
#include "common_fps/wire.hpp"
#include <cassert>
#include <iostream>
#include <limits>

using namespace common_fps;

static void test_shared_rate_validation() {
    assert(validated_fps(30.0) == 30);
    assert(validated_fps(59.6) == 60);
    assert(validated_fps(120.0) == 120);
    assert(validated_fps(125.0) == 120); // sampling jitter, display ceiling
    for (double fps : {200.0, 245.0, 300.0, -1.0, 0.0,
                      std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()})
        assert(!validated_fps(fps));
}

static void test_fixed_dce_loading_and_resume() {
    DceCounterSampler sampler;
    std::array<std::uint64_t, 12> words{};
    std::uint64_t time = 1'000'000;
    words[1] = 1000;
    auto advance = [&](std::uint64_t delta, std::uint64_t us = 1'000'000) {
        words[1] += delta;
        time += us;
        return sampler.sample(words, time);
    };
    assert(!sampler.sample(words, time));
    assert(!advance(300)); // first complete delta is discarded
    assert(advance(60) == 60);
    assert(!advance(245)); // mid-load spike never reaches display
    assert(advance(60) == 60);
    assert(advance(120) == 120);
    assert(advance(126) == 120);
    words[1] = 0; // flip counter reset
    assert(!advance(0));
    assert(advance(30) == 30);
    assert(!advance(500, 5'000'000)); // resume: baseline, no stale delta
    assert(!advance(60));
    assert(advance(60) == 60);
    time -= 2'000'000; // backwards clock rebases instead of freezing
    assert(!sampler.sample(words, time));
    assert(!advance(60));
    assert(advance(60) == 60);
    sampler.reset();
    assert(!sampler.sample(words, time));
    assert(!advance(60));
    assert(advance(30) == 30);
}

static void test_adaptive_dce_and_short_windows() {
    DceCounterSampler sampler;
    std::array<std::uint64_t, 12> words{};
    std::uint64_t time = 1'000'000;
    assert(!sampler.sample(words, time));
    for (unsigned i = 0; i < 3; ++i) {
        time += 1'000'000;
        words[3] += 60;
        words[4] += 300; // a competing counter must be rejected
        const auto fps = sampler.sample(words, time);
        if (i < 2)
            assert(!fps);
        else
            assert(fps == 60);
    }
    assert(sampler.selected_word() == 3);
    words[3] += 6;
    time += 100'000;
    assert(!sampler.sample(words, time)); // keep baseline until >=200 ms
    words[3] += 6;
    time += 100'000;
    assert(sampler.sample(words, time) == 60);
    words[3] += 300;
    time += 1'000'000;
    assert(!sampler.sample(words, time));
    words[3] += 59;
    time += 1'000'000;
    assert(sampler.sample(words, time) == 59);
}

static void test_transport_rejects_invalid_rates() {
    OverlayFrame frame;
    frame.loading = false;
    frame.fps = 300;
    auto packet = make_wire_packet(frame, 1);
    assert(packet.loading && packet.fps == 0);
    packet.loading = 0;
    packet.fps = 245;
    const auto decoded = decode_wire_packet(packet);
    assert(decoded && decoded->loading && decoded->fps == 0);
}

int main() {
    test_shared_rate_validation();
    test_fixed_dce_loading_and_resume();
    test_adaptive_dce_and_short_windows();
    test_transport_rejects_invalid_rates();
    std::cout << "DCE/HEN rate, loading, clock and transport regressions: PASS\n";
}
