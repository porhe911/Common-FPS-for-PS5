/* Common FPS for PS5 - GPL-3.0-or-later */
#include "common_fps/v1_stable_hook_state.hpp"

#include <algorithm>
#include <cstring>

namespace common_fps::v1_stable {

void HookState::store_text(AtomicBuffer& dst, const char* src) {
    for (auto& byte : dst)
        byte.store(0, std::memory_order_relaxed);

    if (!src)
        return;

    const std::size_t len =
        std::min<std::size_t>(std::strlen(src), kHookTextCapacity - 1);

    for (std::size_t i = 0; i < len; ++i) {
        dst[i].store(
            static_cast<unsigned char>(src[i]),
            std::memory_order_relaxed);
    }
    dst[len].store(0, std::memory_order_relaxed);
}

void HookState::load_text(
    const AtomicBuffer& src,
    std::array<char, kHookTextCapacity>& dst) {
    for (std::size_t i = 0; i < kHookTextCapacity; ++i) {
        dst[i] = static_cast<char>(
            src[i].load(std::memory_order_relaxed));
    }
    dst.back() = '\0';
}

void HookState::publish(const char* text, const char* text2) {
    if (!text || !text2)
        return;

    // Writer active: sequence becomes odd.
    sequence_.fetch_add(1, std::memory_order_acq_rel);
    store_text(text_, text);
    store_text(text2_, text2);
    // Publish complete snapshot: sequence becomes even.
    sequence_.fetch_add(1, std::memory_order_release);
}

void HookState::publish_raw(const char* text) {
    if (!text)
        return;

    raw_sequence_.fetch_add(1, std::memory_order_acq_rel);
    store_text(raw_text_, text);
    raw_sequence_.fetch_add(1, std::memory_order_release);
}

bool HookState::snapshot(TextSnapshot& out, unsigned max_attempts) const {
    for (unsigned attempt = 0; attempt < max_attempts; ++attempt) {
        const auto before = sequence_.load(std::memory_order_acquire);
        if (before & 1u)
            continue;

        load_text(text_, out.text);
        load_text(text2_, out.text2);

        const auto after = sequence_.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            out.sequence = after;
            return true;
        }
    }
    return false;
}

bool HookState::snapshot_raw(RawSnapshot& out, unsigned max_attempts) const {
    for (unsigned attempt = 0; attempt < max_attempts; ++attempt) {
        const auto before = raw_sequence_.load(std::memory_order_acquire);
        if (before & 1u)
            continue;

        load_text(raw_text_, out.text);

        const auto after = raw_sequence_.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            out.sequence = after;
            return true;
        }
    }
    return false;
}

std::uint64_t HookState::sequence() const noexcept {
    return sequence_.load(std::memory_order_acquire);
}

std::uint64_t HookState::raw_sequence() const noexcept {
    return raw_sequence_.load(std::memory_order_acquire);
}

} // namespace common_fps::v1_stable
