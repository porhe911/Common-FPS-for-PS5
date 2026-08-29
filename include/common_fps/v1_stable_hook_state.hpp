/* Common FPS for PS5 - GPL-3.0-or-later */
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace common_fps::v1_stable {

inline constexpr std::size_t kHookTextCapacity = 1024;

struct TextSnapshot {
    std::uint64_t sequence = 0;
    std::array<char, kHookTextCapacity> text{};
    std::array<char, kHookTextCapacity> text2{};
};

struct RawSnapshot {
    std::uint64_t sequence = 0;
    std::array<char, kHookTextCapacity> text{};
};

/*
 * Source reconstruction of the state bridge used by the stable renderer.
 *
 * The donor binary uses an odd/even sequence protocol around fixed 0x400-byte
 * buffers. OnRender reads only a stable even sequence and retries if a writer
 * changed the sequence during the copy. This keeps UI mutation on the render
 * thread while the UDP receiver only publishes text state.
 */
class HookState {
public:
    void publish(const char* text, const char* text2);
    void publish_raw(const char* text);

    bool snapshot(TextSnapshot& out, unsigned max_attempts = 16) const;
    bool snapshot_raw(RawSnapshot& out, unsigned max_attempts = 16) const;

    std::uint64_t sequence() const noexcept;
    std::uint64_t raw_sequence() const noexcept;

private:
    static void copy_text(std::array<char, kHookTextCapacity>& dst,
                          const char* src);

    mutable std::atomic<std::uint64_t> sequence_{0};
    mutable std::atomic<std::uint64_t> raw_sequence_{0};
    std::array<char, kHookTextCapacity> text_{};
    std::array<char, kHookTextCapacity> text2_{};
    std::array<char, kHookTextCapacity> raw_text_{};
};

} // namespace common_fps::v1_stable
