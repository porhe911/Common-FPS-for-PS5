#include "common_fps/v1_stable_wire.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>

int main() {
    using namespace common_fps::v1_stable;

    static_assert(kWireMagic == 0x46554850u);
    static_assert(kWireVersion == 1);
    static_assert(kWirePort == 55541);
    static_assert(sizeof(FpsPacket) == 0x420);
    static_assert(offsetof(FpsPacket, last_ns) == 0x18);
    static_assert(offsetof(FpsPacket, text) == 0x20);

    FpsPacket packet{};
    set_loading(packet, 7);
    assert(packet.magic == kWireMagic);
    assert(packet.version == 1);
    assert(packet.sequence == 7);
    assert(packet.fps == 0.0);
    assert(packet.last_ns == 0);
    assert(std::strcmp(packet.text, "FPS\tloading\n") == 0);

    set_numeric(packet, 8, 59.9);
    assert(packet.sequence == 8);
    assert(std::fabs(packet.fps - 59.9) < 0.000001);
    assert(packet.last_ns == 0);
    assert(packet.text[0] == '\0');

    const auto fps60 = calculate_fps(1000, 1060, 1000000);
    assert(fps60.has_value());
    assert(std::fabs(*fps60 - 60.0) < 0.000001);

    const auto fps2999 = calculate_fps(0, 2999, 10000000);
    assert(fps2999.has_value());
    assert(std::fabs(*fps2999 - 299.9) < 0.000001);

    const auto fpsTooHigh = calculate_fps(0, 301, 1000000);
    assert(!fpsTooHigh.has_value());

    const auto noTime = calculate_fps(0, 60, 0);
    assert(!noTime.has_value());

    // The stable code subtracts uint32_t counters, so wrap-around is natural.
    const auto wrapped = calculate_fps(0xfffffff0u, 0x0000000eu, 1000000);
    assert(wrapped.has_value());
    assert(std::fabs(*wrapped - 30.0) < 0.000001);

    return 0;
}
