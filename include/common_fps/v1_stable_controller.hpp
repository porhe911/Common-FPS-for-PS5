/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include "common_fps/platform.hpp"
#include "common_fps/v1_stable_sampler.hpp"
#include "common_fps/v1_stable_wire.hpp"

#include <cstdint>

namespace common_fps::v1_stable {

class PacketSink {
public:
    virtual ~PacketSink() = default;
    virtual bool send(const FpsPacket& packet) = 0;
};

/*
 * Host-testable reconstruction of stable probe_main_entry's game lifecycle.
 * The PS5 entry point is responsible for installing the stable renderer before
 * entering this loop; this class only performs game detection/sampling and
 * PHUF packet publication.
 */
class Controller {
public:
    Controller(Platform& platform, PacketSink& sink);

    // Perform one stable one-second lifecycle iteration.
    bool tick();

    std::uint64_t next_sequence() const noexcept { return sequence_; }
    bool attached() const noexcept { return sampler_.attached(); }

private:
    bool send_loading();

    Platform& platform_;
    PacketSink& sink_;
    Sampler sampler_;
    std::uint64_t sequence_ = 1;
};

} // namespace common_fps::v1_stable
