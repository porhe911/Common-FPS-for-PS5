/*
 * Common FPS for PS5
 * Hardware parity probe for the reconstructed stable v1.0.0 FPS sampler.
 *
 * This diagnostic ELF intentionally does not install the ShellUI renderer and
 * does not replace the shipping controller/plugin. It exits after the first
 * valid FPS sample (or after a bounded timeout) so FW 9.60 hardware can verify
 * the PHU-style DMAP read path in isolation.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/v1_stable_controller.hpp"
#include "v1_stable_ps5_platform.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>

extern "C" {

typedef struct notify_request {
    char padding[45];
    char message[3075];
} notify_request_t;

int sceKernelSendNotificationRequest(
    int device,
    notify_request_t* request,
    std::size_t request_size,
    int flags);
}

namespace {

void notify(const char* message) {
    notify_request_t request{};
    std::snprintf(request.message, sizeof(request.message), "%s", message);
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

class ParityProbeSink final : public common_fps::v1_stable::PacketSink {
public:
    bool send(const common_fps::v1_stable::FpsPacket& packet) override {
        if (done_)
            return true;

        if (packet.magic != common_fps::v1_stable::kWireMagic ||
            packet.version != common_fps::v1_stable::kWireVersion) {
            return false;
        }

        // Loading packets carry text. Numeric stable-v1 packets leave it empty.
        if (packet.text[0] != '\0')
            return true;

        char message[192]{};
        std::snprintf(
            message,
            sizeof(message),
            "Common FPS parity probe\nDMAP FW 9.60 OK: %.1f FPS",
            packet.fps);
        notify(message);
        done_ = true;
        return true;
    }

    [[nodiscard]] bool done() const noexcept { return done_; }

private:
    bool done_ = false;
};

} // namespace

int main() {
    common_fps::ps5::V1StablePs5Platform platform;
    ParityProbeSink sink;
    common_fps::v1_stable::Controller controller(platform, sink);

    // Controller::tick() is the reconstructed stable one-second lifecycle.
    // 90 iterations gives enough time to launch the probe just before a game.
    for (unsigned attempt = 0; attempt < 90 && !sink.done(); ++attempt)
        controller.tick();

    if (!sink.done()) {
        notify(
            "Common FPS parity probe\n"
            "No valid FPS after 90 seconds");
        return 2;
    }

    // Keep the payload alive briefly so the notification request is consumed.
    platform.sleep_ms(1500);
    return 0;
}
