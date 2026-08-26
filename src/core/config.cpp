/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "common_fps/config.hpp"
#include "common_fps/constants.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace common_fps {

static std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

static Corner parse_corner(const std::string& value) {
    if (value == "top_left") return Corner::TopLeft;
    if (value == "top_right") return Corner::TopRight;
    if (value == "bottom_right") return Corner::BottomRight;
    return Corner::BottomLeft;
}

const char* corner_name(Corner corner) {
    switch (corner) {
        case Corner::TopLeft: return "top_left";
        case Corner::TopRight: return "top_right";
        case Corner::BottomRight: return "bottom_right";
        case Corner::BottomLeft:
        default: return "bottom_left";
    }
}

OverlayConfig default_config() {
    return {};
}

OverlayConfig parse_config_text(std::string_view text) {
    OverlayConfig cfg = default_config();
    std::istringstream in{std::string(text)};
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;

        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        auto key = trim(line.substr(0, pos));
        auto value = trim(line.substr(pos + 1));

        if (key == "corner") {
            cfg.corner = parse_corner(value);
        } else if (key == "font_size") {
            const int v = std::atoi(value.c_str());
            cfg.font_size = std::clamp(v, kMinFontSize, kMaxFontSize);
        } else if (key == "margin_x") {
            cfg.margin_x = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
        } else if (key == "margin_y") {
            cfg.margin_y = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
        }
    }

    return cfg;
}

std::string serialize_config(const OverlayConfig& cfg) {
    std::ostringstream out;
    out << "[overlay]\n";
    out << "corner=" << corner_name(cfg.corner) << "\n";
    out << "font_size=" << cfg.font_size << "\n";
    out << "margin_x=" << cfg.margin_x << "\n";
    out << "margin_y=" << cfg.margin_y << "\n";
    return out.str();
}

} // namespace common_fps
