/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/settings.hpp"
#include "common_fps/config.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

namespace common_fps {
namespace {
bool atomic_write(const std::string& path, const std::string& data) {
    const std::string pending = path + ".tmp";
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int fd = open(pending.c_str(), flags, 0600);
    if (fd < 0) return false;
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto n = write(fd, data.data() + offset, data.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        offset += static_cast<std::size_t>(n);
    }
    bool ok = offset == data.size() && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(pending.c_str(), path.c_str()) == 0;
    if (!ok) unlink(pending.c_str());
    return ok;
}

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<std::string> unescape(std::string_view text) {
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        if (c == '%') {
            if (i + 2 >= text.size() || hex(text[i+1]) < 0 || hex(text[i+2]) < 0)
                return std::nullopt;
            c = static_cast<unsigned char>(hex(text[i+1]) * 16 + hex(text[i+2]));
            i += 2;
        } else if (c == '+') c = ' ';
        if (c < 32 || c > 126) return std::nullopt;
        result.push_back(static_cast<char>(c));
    }
    return result;
}

bool integer(std::string_view s, int min, int max, int& value) {
    if (s.empty() || s.size() > 3) return false;
    int n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + c - '0';
    }
    if (n < min || n > max) return false;
    value = n;
    return true;
}

std::string response(int status, const char* reason, std::string_view body,
                     const char* type = "text/plain; charset=utf-8") {
    return "HTTP/1.1 " + std::to_string(status) + " " + reason +
        "\r\nContent-Type: " + type + "\r\nContent-Length: " +
        std::to_string(body.size()) +
        "\r\nConnection: close\r\nCache-Control: no-store"
        "\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY"
        "\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; "
        "style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'"
        "\r\n\r\n" + std::string(body);
}
} // namespace

SettingsStore::SettingsStore(std::string path) : path_(std::move(path)) {
    FILE* f = fopen(path_.c_str(), "rb");
    if (!f) return;
    char data[kMaxSettingsBytes + 1]{};
    const auto n = fread(data, 1, sizeof(data), f);
    const bool ok = !ferror(f) && n <= kMaxSettingsBytes;
    fclose(f);
    if (ok) config_ = parse_config_text(std::string_view(data, n));
}

SettingsStore::~SettingsStore() { pthread_mutex_destroy(&mutex_); }

OverlayConfig SettingsStore::get() {
    pthread_mutex_lock(&mutex_);
    const auto copy = config_;
    pthread_mutex_unlock(&mutex_);
    return copy;
}

bool SettingsStore::save(const OverlayConfig& config) {
    const auto next = sanitize_config(config);
    pthread_mutex_lock(&mutex_);
    const bool ok = atomic_write(path_, serialize_config(next));
    if (ok) config_ = next;
    pthread_mutex_unlock(&mutex_);
    return ok;
}

std::optional<OverlayConfig> parse_settings_form(std::string_view body) {
    if (body.empty() || body.size() > 512) return std::nullopt;
    std::map<std::string, std::string> fields;
    while (!body.empty()) {
        const auto amp = body.find('&');
        const auto part = body.substr(0, amp);
        const auto eq = part.find('=');
        if (eq == std::string_view::npos) return std::nullopt;
        const auto key = unescape(part.substr(0, eq));
        const auto value = unescape(part.substr(eq+1));
        if (!key || !value || !fields.emplace(*key, *value).second)
            return std::nullopt;
        if (amp == std::string_view::npos) break;
        body.remove_prefix(amp + 1);
        if (body.empty()) return std::nullopt;
    }
    constexpr const char* keys[] = {"corner", "font_size", "margin_x", "margin_y",
                                  "show_label", "label_color", "value_color"};
    if (fields.size() != 7) return std::nullopt;
    for (const char* k : keys) if (fields.count(k) != 1) return std::nullopt;
    OverlayConfig cfg;
    const auto& corner = fields["corner"];
    if (corner == "top_left") cfg.corner = Corner::TopLeft;
    else if (corner == "top_right") cfg.corner = Corner::TopRight;
    else if (corner == "bottom_left") cfg.corner = Corner::BottomLeft;
    else if (corner == "bottom_right") cfg.corner = Corner::BottomRight;
    else return std::nullopt;
    int mx = 0, my = 0, label = 0;
    if (!integer(fields["font_size"], kMinFontSize, kMaxFontSize, cfg.font_size) ||
        !integer(fields["margin_x"], 0, 240, mx) ||
        !integer(fields["margin_y"], 0, 240, my) ||
        !integer(fields["show_label"], 0, 1, label)) return std::nullopt;
    cfg.margin_x = static_cast<float>(mx);
    cfg.margin_y = static_cast<float>(my);
    cfg.show_label = label != 0;
    for (const char* key : {"label_color", "value_color"}) {
        const auto& color = fields[key];
        if (color.size() != 7 || color[0] != '#') return std::nullopt;
        for (std::size_t i = 1; i < 7; ++i) if (hex(color[i]) < 0) return std::nullopt;
        const auto rgb = static_cast<std::uint32_t>(std::strtoul(color.c_str()+1, nullptr, 16));
        if (key[0] == 'l') cfg.label_color = rgb; else cfg.value_color = rgb;
    }
    return cfg;
}

std::string settings_http_response(std::string_view request, SettingsStore& store,
                                  std::string_view page) {
    const auto bad = [] { return response(400, "Bad Request", "Invalid request\n"); };
    if (request.size() > kMaxSettingsBytes) return bad();
    const auto split = request.find("\r\n\r\n");
    const auto line_end = request.find("\r\n");
    if (split == std::string_view::npos || line_end == std::string_view::npos) return bad();
    const auto line = request.substr(0, line_end);
    const auto space = line.find(' '), last = line.rfind(' ');
    if (space == last || last == std::string_view::npos ||
        (line.substr(last+1) != "HTTP/1.1" && line.substr(last+1) != "HTTP/1.0")) return bad();
    const auto method = line.substr(0, space), path = line.substr(space+1, last-space-1);
    std::map<std::string, std::string> headers;
    std::size_t pos = line_end + 2;
    while (pos < split) {
        const auto end = request.find("\r\n", pos);
        const auto h = request.substr(pos, end-pos);
        const auto colon = h.find(':');
        if (colon == std::string_view::npos) return bad();
        std::string key(h.substr(0, colon)), value(h.substr(colon+1));
        for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        while (!value.empty() && value.front() == ' ') value.erase(0, 1);
        while (!value.empty() && value.back() == ' ') value.pop_back();
        if (!headers.emplace(key, value).second) return bad();
        pos = end+2;
    }
    if (headers["host"] != kSettingsHost) return response(403, "Forbidden", "Invalid host\n");
    if (headers.count("transfer-encoding")) return bad();
    const auto body = request.substr(split+4);
    int length = 0;
    if (headers.count("content-length") &&
        !integer(headers["content-length"], 0, 512, length)) return bad();
    if (static_cast<std::size_t>(length) != body.size()) return bad();
    if (method == "GET" && path == "/")
        return response(200, "OK", page, "text/html; charset=utf-8");
    if (method == "GET" && path == "/api/config")
        return response(200, "OK", serialize_config(store.get()));
    if (method == "POST" && path == "/api/config") {
        if (headers["x-commonfps"] != "1" ||
            headers["content-type"] != "application/x-www-form-urlencoded" ||
            (headers.count("origin") && headers["origin"] != std::string("http://") + kSettingsHost))
            return response(403, "Forbidden", "Invalid origin\n");
        const auto config = parse_settings_form(body);
        if (!config) return response(422, "Unprocessable Entity", "Invalid settings\n");
        if (!store.save(*config)) return response(500, "Internal Server Error", "Cannot save settings\n");
        return response(200, "OK", serialize_config(store.get()));
    }
    return response(404, "Not Found", "Not found\n");
}
} // namespace common_fps
