#include "stable_producer.hpp"

#include "phuf_wire.hpp"
#include "process_sysctl.hpp"
#include "videoout_counter.hpp"

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace common_fps::legacy_v028b {
namespace {

constexpr std::uint32_t kMaxTenthsFps = 3000U;
constexpr std::uint64_t kTenthsScale = 10'000'000ULL;
constexpr double kTenthsToFps = 10.0;
constexpr char kLoadingText[] = "FPS\tloading\n";

std::uint16_t network_port_literal() noexcept {
    // htons(55541) on the x86-64 little-endian target. The reference binary
    // stores 0xF5D8 in the sockaddr field, producing network bytes D8 F5.
    return 0xF5D8U;
}

class StatePublisher {
public:
    StatePublisher() noexcept {
        packet_.magic = kPhufMagic;
        packet_.version = kPhufVersion;
        packet_.sequence = 0;
        packet_.fps = 0.0;
        packet_.reserved = 0;
        std::memset(packet_.text, 0, sizeof(packet_.text));

        std::memset(&destination_, 0, sizeof(destination_));
        destination_.sin_family = AF_INET;
        destination_.sin_port = network_port_literal();
        // 127.0.0.1 bytes in target memory: 7f 00 00 01.
        destination_.sin_addr.s_addr = 0x0100007FU;

        while ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
            sleep(1);
    }

    void publish_loading() noexcept {
        packet_.fps = 0.0;
        std::memset(packet_.text, 0, sizeof(packet_.text));
        std::memcpy(packet_.text, kLoadingText, sizeof(kLoadingText));
        send_current();
    }

    void publish_fps(double fps) noexcept {
        packet_.fps = fps;
        packet_.text[0] = '\0';
        send_current();
    }

private:
    void send_current() noexcept {
        packet_.sequence = next_sequence_++;
        (void)sendto(
            socket_,
            &packet_,
            sizeof(packet_),
            0,
            reinterpret_cast<const sockaddr*>(&destination_),
            sizeof(destination_));
    }

    int socket_ = -1;
    std::uint64_t next_sequence_ = 1;
    sockaddr_in destination_{};
    PhufStatePacket packet_{};
};

std::int64_t elapsed_microseconds(
    const timeval& before,
    const timeval& after) noexcept {
    return static_cast<std::int64_t>(after.tv_sec - before.tv_sec) *
               1'000'000LL +
           static_cast<std::int64_t>(after.tv_usec - before.tv_usec);
}

} // namespace

[[noreturn]] void run_stable_producer_loop() noexcept {
    StatePublisher publisher;

    for (;;) {
        const pid_t pid = find_game_pid_sysctl();
        if (pid < 0) {
            publisher.publish_loading();
            sleep(1);
            continue;
        }

        const auto counter_address = resolve_videoout_counter(pid);
        if (!counter_address) {
            publisher.publish_loading();
            sleep(1);
            continue;
        }

        std::uint32_t previous_counter = 0;
        if (!read_videoout_counter(pid, *counter_address, previous_counter)) {
            publisher.publish_loading();
            sleep(1);
            continue;
        }

        timeval previous_time{};
        gettimeofday(&previous_time, nullptr);
        sleep(1);

        for (;;) {
            const pid_t current_pid = find_game_pid_sysctl();
            if (current_pid < 0 || current_pid != pid) {
                publisher.publish_loading();
                break;
            }

            std::uint32_t current_counter = 0;
            if (!read_videoout_counter(pid, *counter_address, current_counter)) {
                publisher.publish_loading();
                break;
            }

            timeval current_time{};
            gettimeofday(&current_time, nullptr);

            const std::int64_t elapsed =
                elapsed_microseconds(previous_time, current_time);

            if (elapsed > 0) {
                const std::uint32_t delta =
                    static_cast<std::uint32_t>(
                        current_counter - previous_counter);
                const std::uint64_t scaled =
                    static_cast<std::uint64_t>(delta) * kTenthsScale;
                const std::uint32_t tenths = static_cast<std::uint32_t>(
                    scaled / static_cast<std::uint64_t>(elapsed));

                if (tenths > kMaxTenthsFps) {
                    publisher.publish_loading();
                    break;
                }

                publisher.publish_fps(
                    static_cast<double>(tenths) / kTenthsToFps);
            }

            previous_counter = current_counter;
            previous_time = current_time;
            sleep(1);
        }
    }
}

} // namespace common_fps::legacy_v028b
