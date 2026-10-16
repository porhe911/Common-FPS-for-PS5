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
#include <cmath>
#include <iomanip>
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

static bool number(const std::string& text, float& result) {
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || *end || !std::isfinite(value))
        return false;
    result = value;
    return true;
}

static void parse_color(std::string text, std::uint32_t& result) {
    if (!text.empty() && text.front() == '#') text.erase(0, 1);
    if (text.size() != 6 || text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
        return;
    result = static_cast<std::uint32_t>(std::strtoul(text.c_str(), nullptr, 16));
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
            float v = 0;
            if (number(value, v))
                cfg.font_size = static_cast<int>(std::clamp(v, float(kMinFontSize), float(kMaxFontSize)));
        } else if (key == "margin_x") {
            (void)number(value, cfg.margin_x);
        } else if (key == "margin_y") {
            (void)number(value, cfg.margin_y);
        } else if (key == "show_label") {
            if (value == "0" || value == "false") cfg.show_label = false;
            if (value == "1" || value == "true") cfg.show_label = true;
        } else if (key == "label_color") {
            parse_color(value, cfg.label_color);
        } else if (key == "value_color") {
            parse_color(value, cfg.value_color);
        }
    }

    return sanitize_config(cfg);
}

std::string serialize_config(const OverlayConfig& cfg) {
    std::ostringstream out;
    out << "[overlay]\n";
    out << "corner=" << corner_name(cfg.corner) << "\n";
    out << "font_size=" << cfg.font_size << "\n";
    out << "margin_x=" << cfg.margin_x << "\n";
    out << "margin_y=" << cfg.margin_y << "\n";
    out << "show_label=" << (cfg.show_label ? 1 : 0) << "\n";
    out << std::hex << std::uppercase << std::setfill('0');
    out << "label_color=#" << std::setw(6) << (cfg.label_color & 0xFFFFFF) << "\n";
    out << "value_color=#" << std::setw(6) << (cfg.value_color & 0xFFFFFF) << "\n";
    return out.str();
}

} // namespace common_fps
