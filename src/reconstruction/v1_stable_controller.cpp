/* Common FPS for PS5 - GPL-3.0-or-later */
#include "common_fps/v1_stable_controller.hpp"

namespace common_fps::v1_stable {

Controller::Controller(Platform& platform, PacketSink& sink)
    : platform_(platform), sink_(sink), sampler_(platform) {}

bool Controller::send_loading() {
    FpsPacket packet{};
    set_loading(packet, sequence_++);
    return sink_.send(packet);
}

bool Controller::tick() {
    if (!sampler_.attached()) {
        const auto pid = platform_.find_game_process();
        if (!pid) {
            send_loading();
            platform_.sleep_ms(1000);
            return true;
        }

        if (!sampler_.attach(*pid)) {
            send_loading();
            platform_.sleep_ms(1000);
            return true;
        }
    }

    const auto fps = sampler_.sample();
    if (!fps) {
        send_loading();
        platform_.sleep_ms(1000);
        return true;
    }

    FpsPacket packet{};
    set_numeric(packet, sequence_++, *fps);
    sink_.send(packet);
    platform_.sleep_ms(1000);
    return true;
}

} // namespace common_fps::v1_stable
