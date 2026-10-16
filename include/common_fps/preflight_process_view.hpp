/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace common_fps {

struct PreflightProcessView {
    std::size_t size = 0;
    std::int32_t pid = -1;
    std::int32_t ppid = -1;
    char legacy_name[33]{};
    int game_name_offset = -1;
    int shellui_name_offset = -1;
};

/* Diagnostic evidence only: no guessed field is used as a patch target. */
inline int preflight_name_offset(
    const std::uint8_t* bytes, std::size_t size, const char* name) noexcept {
    const std::size_t count = std::strlen(name) + 1;
    if (size < count)
        return -1;
    for (std::size_t i = 0; i <= size - count; ++i) {
        if ((i == 0 || bytes[i - 1] == 0) &&
            std::memcmp(bytes + i, name, count) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

inline bool preflight_process_view(
    const std::uint8_t* bytes, std::size_t remaining,
    PreflightProcessView& view) noexcept {
    view = {};
    if (!bytes || remaining < sizeof(std::int32_t))
        return false;
    std::int32_t length = 0;
    std::memcpy(&length, bytes, sizeof(length));
    if (length < 80 || static_cast<std::size_t>(length) > remaining ||
        length > 64 * 1024)
        return false;
    view.size = static_cast<std::size_t>(length);
    std::memcpy(&view.pid, bytes + 72, sizeof(view.pid));
    std::memcpy(&view.ppid, bytes + 76, sizeof(view.ppid));
    constexpr std::size_t legacy_offset = 447;
    if (view.size > legacy_offset) {
        const std::size_t count = std::min(
            view.size - legacy_offset, sizeof(view.legacy_name) - 1);
        for (std::size_t i = 0; i < count; ++i) {
            const unsigned char c = bytes[legacy_offset + i];
            if (c == 0)
                break;
            view.legacy_name[i] = c >= 32 && c <= 126 ? c : '.';
        }
    }
    view.game_name_offset = preflight_name_offset(bytes, view.size, "eboot.bin");
    view.shellui_name_offset = preflight_name_offset(bytes, view.size, "SceShellUI");
    return true;
}

} // namespace common_fps
