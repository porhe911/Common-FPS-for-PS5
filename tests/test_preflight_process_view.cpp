/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/preflight_process_view.hpp"
#include "common_fps/firmware_runtime_policy.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

using namespace common_fps;

int main() {
    assert(firmware_is_fw1160(0x11600000U));
    assert(firmware_is_fw1160(0x11600045U));
    assert(!firmware_is_fw1160(0x10600000U));
    assert(!firmware_is_fw1160(0x09600000U));
    assert(!firmware_is_fw1160(0x09000000U));
    assert(!firmware_is_fw1160(0x04510000U));
    std::array<std::uint8_t, 1096> record{};
    std::int32_t length = record.size(), pid = 123, ppid = 84;
    std::memcpy(record.data(), &length, 4);
    std::memcpy(record.data() + 72, &pid, 4);
    std::memcpy(record.data() + 76, &ppid, 4);
    std::memcpy(record.data() + 447, "eboot.bin", 10);
    PreflightProcessView view{};
    assert(preflight_process_view(record.data(), record.size(), view));
    assert(view.pid == 123 && view.ppid == 84 && view.size == 1096);
    assert(std::strcmp(view.legacy_name, "eboot.bin") == 0);
    assert(view.game_name_offset == 447 && view.shellui_name_offset == -1);

    // Discover a changed name position as evidence without using it to inject.
    std::memset(record.data() + 447, 0, 10);
    std::memcpy(record.data() + 800, "SceShellUI", 11);
    assert(preflight_process_view(record.data(), record.size(), view));
    assert(view.shellui_name_offset == 800 && view.game_name_offset == -1);

    // No prefix match, unterminated match, or read beyond the returned record.
    std::memcpy(record.data() + 900, "eboot.bin-extra", 15);
    assert(preflight_process_view(record.data(), record.size(), view));
    assert(view.game_name_offset == -1);
    assert(!preflight_process_view(record.data(), record.size() - 1, view));
    assert(!preflight_process_view(nullptr, record.size(), view));
    length = 79;
    std::memcpy(record.data(), &length, 4);
    assert(!preflight_process_view(record.data(), record.size(), view));
    length = -1;
    std::memcpy(record.data(), &length, 4);
    assert(!preflight_process_view(record.data(), record.size(), view));
    std::cout << "Bounded diagnostic process parsing: PASS\n";
}
