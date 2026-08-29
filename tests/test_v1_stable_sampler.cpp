#include "common_fps/constants.hpp"
#include "common_fps/v1_stable_sampler.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

namespace {
class MockPlatform final : public common_fps::Platform {
public:
    std::optional<common_fps::ProcessId> active_pid;
    bool alive = false;
    std::uintptr_t module_base = 0x10000000;
    std::uintptr_t record_pointer = 0x20000000;
    std::uintptr_t counter_root = 0x30000000;
    std::uint32_t counter = 1000;
    std::uint64_t now_us = 1000000;

    std::optional<common_fps::ProcessId> find_game_process() override {
        return active_pid;
    }
    bool process_alive(common_fps::ProcessId pid) override {
        return alive && active_pid && *active_pid == pid;
    }
    std::optional<common_fps::ModuleInfo>
    find_module(common_fps::ProcessId pid, const char* name) override {
        if (!process_alive(pid))
            return std::nullopt;
        return common_fps::ModuleInfo{module_base, name};
    }
    bool read_memory(common_fps::ProcessId pid,
                     std::uintptr_t address,
                     void* out,
                     std::size_t size) override {
        using namespace common_fps;
        if (!process_alive(pid))
            return false;

        if (address == module_base + kVideoOutProbeTableOffset) {
            const std::size_t expected =
                kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;
            assert(size == expected);
            std::vector<std::uint8_t> table(expected, 0);
            const std::uint32_t enabled = 1;
            std::memcpy(table.data(), &enabled, sizeof(enabled));
            std::memcpy(table.data() + 8, &record_pointer,
                        sizeof(record_pointer));
            std::memcpy(out, table.data(), expected);
            return true;
        }
        if (address == record_pointer) {
            std::memcpy(out, &counter_root, sizeof(counter_root));
            return true;
        }
        if (address == counter_root + kVideoOutCounterOffset) {
            std::memcpy(out, &counter, sizeof(counter));
            return true;
        }
        return false;
    }
    std::uint64_t monotonic_us() override { return now_us; }
    void sleep_ms(unsigned ms) override { now_us += ms * 1000ull; }

    void advance(std::uint32_t frames, std::uint64_t us) {
        counter += frames;
        now_us += us;
    }
};
}

int main() {
    using common_fps::v1_stable::Sampler;

    MockPlatform p;
    p.active_pid = 100;
    p.alive = true;

    Sampler sampler(p);
    assert(sampler.attach(100));
    assert(sampler.attached());

    // First reading only establishes the stable baseline.
    assert(!sampler.sample().has_value());

    // Preserve tenth-FPS precision exactly as v1.0.0 did on its wire.
    p.advance(596, 10000000);
    const auto value = sampler.sample();
    assert(value.has_value());
    assert(std::fabs(*value - 59.6) < 0.000001);

    // Process exit resets attachment state.
    p.alive = false;
    assert(!sampler.sample().has_value());
    assert(!sampler.attached());

    return 0;
}
