#include "common_fps/constants.hpp"
#include "common_fps/v1_stable_controller.hpp"

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
    std::uintptr_t pointer = 0x20000000;
    std::uintptr_t root = 0x30000000;
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
        if (!process_alive(pid)) return std::nullopt;
        return common_fps::ModuleInfo{module_base, name};
    }
    bool read_memory(common_fps::ProcessId pid, std::uintptr_t addr,
                     void* out, std::size_t size) override {
        using namespace common_fps;
        if (!process_alive(pid)) return false;
        if (addr == module_base + kVideoOutProbeTableOffset) {
            const std::size_t n =
                kVideoOutProbeEntryCount * kVideoOutProbeEntrySize;
            assert(size == n);
            std::vector<std::uint8_t> table(n, 0);
            const std::uint32_t enabled = 1;
            std::memcpy(table.data(), &enabled, 4);
            std::memcpy(table.data() + 8, &pointer, 8);
            std::memcpy(out, table.data(), n);
            return true;
        }
        if (addr == pointer) {
            std::memcpy(out, &root, sizeof(root));
            return true;
        }
        if (addr == root + kVideoOutCounterOffset) {
            std::memcpy(out, &counter, sizeof(counter));
            return true;
        }
        return false;
    }
    std::uint64_t monotonic_us() override { return now_us; }
    void sleep_ms(unsigned ms) override { now_us += ms * 1000ull; }
};

class Sink final : public common_fps::v1_stable::PacketSink {
public:
    std::vector<common_fps::v1_stable::FpsPacket> packets;
    bool send(const common_fps::v1_stable::FpsPacket& p) override {
        packets.push_back(p);
        return true;
    }
};
}

int main() {
    using namespace common_fps::v1_stable;

    MockPlatform p;
    Sink sink;
    Controller controller(p, sink);

    // No game: stable loading packet.
    controller.tick();
    assert(sink.packets.size() == 1);
    assert(std::string(sink.packets.back().raw) == "FPS\tloading\n");
    assert(sink.packets.back().sequence == 1);

    // Game appears: first sample establishes baseline and remains loading.
    p.active_pid = 100;
    p.alive = true;
    controller.tick();
    assert(controller.attached());
    assert(sink.packets.back().sequence == 2);
    assert(std::string(sink.packets.back().raw) == "FPS\tloading\n");

    // 60 counter increments over the one-second controller interval.
    p.counter += 60;
    controller.tick();
    assert(sink.packets.back().sequence == 3);
    assert(sink.packets.back().raw[0] == '\0');
    assert(std::fabs(sink.packets.back().fps - 60.0) < 0.000001);

    // Exit resets sampling; next iteration returns to loading.
    p.alive = false;
    controller.tick();
    assert(std::string(sink.packets.back().raw) == "FPS\tloading\n");
    assert(!controller.attached());

    return 0;
}
