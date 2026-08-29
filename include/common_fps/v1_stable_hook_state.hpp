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
 * The historical binary uses an odd/even sequence protocol around fixed
 * 0x400-byte buffers. This source version keeps the same protocol while using
 * atomic bytes so the behavior is also well-defined under the C++ memory model.
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
    using AtomicBuffer =
        std::array<std::atomic<unsigned char>, kHookTextCapacity>;

    static void store_text(AtomicBuffer& dst, const char* src);
    static void load_text(const AtomicBuffer& src,
                          std::array<char, kHookTextCapacity>& dst);

    mutable std::atomic<std::uint64_t> sequence_{0};
    mutable std::atomic<std::uint64_t> raw_sequence_{0};
    AtomicBuffer text_{};
    AtomicBuffer text2_{};
    AtomicBuffer raw_text_{};
};

} // namespace common_fps::v1_stable
