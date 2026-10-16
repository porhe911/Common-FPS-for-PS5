/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "common_fps/types.hpp"
#include <optional>
#include <pthread.h>
#include <string>
#include <string_view>

namespace common_fps {
inline constexpr unsigned kSettingsPort = 39029;
inline constexpr std::size_t kMaxSettingsBytes = 4096;
inline constexpr char kSettingsPath[] = "/data/CommonFPS/config.ini";
inline constexpr char kSettingsHost[] = "127.0.0.1:39029";

class SettingsStore {
public:
    explicit SettingsStore(std::string path);
    ~SettingsStore();
    SettingsStore(const SettingsStore&) = delete;
    SettingsStore& operator=(const SettingsStore&) = delete;
    OverlayConfig get();
    bool save(const OverlayConfig& config);
private:
    std::string path_;
    OverlayConfig config_{};
    pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
};

std::optional<OverlayConfig> parse_settings_form(std::string_view body);

// Complete bounded HTTP request. No filesystem paths or executable operations.
std::string settings_http_response(std::string_view request,
                                  SettingsStore& store,
                                  std::string_view page);
} // namespace common_fps
