/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/config.hpp"
#include "common_fps/layout.hpp"
#include "common_fps/settings.hpp"
#include "common_fps/wire.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>

using namespace common_fps;

static const std::string form =
    "corner=top_right&font_size=40&margin_x=22&margin_y=18&show_label=0&"
    "label_color=%2373D8FF&value_color=%2376F5AA";

static std::string request(const std::string& method, const std::string& path,
                           const std::string& body = {},
                           const std::string& extra = "X-CommonFPS: 1\r\n") {
    return method+" "+path+" HTTP/1.1\r\nHost: 127.0.0.1:39029\r\n"+
        "Content-Type: application/x-www-form-urlencoded\r\n"+
        "Content-Length: "+std::to_string(body.size())+"\r\n"+extra+"\r\n"+body;
}

static void status(const std::string& response, int code) {
    assert(response.find("HTTP/1.1 "+std::to_string(code)+" ") == 0);
    const auto start = response.find("Content-Length: ")+16;
    const auto body = response.find("\r\n\r\n")+4;
    assert(std::stoul(response.substr(start)) == response.size()-body);
}

static void test_validation() {
    const auto cfg = parse_settings_form(form);
    assert(cfg && cfg->font_size == 40 && !cfg->show_label && cfg->value_color == 0x76F5AA);
    assert(same_config(*cfg, parse_config_text(serialize_config(*cfg))));
    for (const auto& bad : {form+"&corner=top_left", form+"&other=1",
            std::string("corner=top_left"), std::string(600, 'a')})
        assert(!parse_settings_form(bad));
    for (const auto& pair : {std::make_pair("font_size=40", "font_size=999"),
        {"font_size=40", "font_size=NaN"}, {"margin_x=22", "margin_x=-1"},
        {"corner=top_right", "corner=garbage"}, {"show_label=0", "show_label=2"},
        {"%2373D8FF", "%23GGGGGG"}, {"%2373D8FF", "%0073D8FF"}}) {
        std::string bad = form;
        bad.replace(bad.find(pair.first), std::string(pair.first).size(), pair.second);
        assert(!parse_settings_form(bad));
    }
    auto bad = parse_config_text("font_size=bad\nmargin_x=nan\nmargin_y=inf\nvalue_color=banana\n");
    assert(same_config(bad, default_config()));
    bad.margin_x = std::numeric_limits<float>::infinity();
    bad.margin_y = -100;
    const auto safe = sanitize_config(bad);
    assert(std::isfinite(safe.margin_x) && safe.margin_y == 0);
}

static void test_all_scenes_and_wire() {
    for (auto scene : {Vec2{1280,720}, Vec2{1920,1080}, Vec2{2560,1440},
                       Vec2{3840,2160}, Vec2{3440,1440}, Vec2{640,360}}) {
        for (auto corner : {Corner::TopLeft, Corner::TopRight, Corner::BottomLeft, Corner::BottomRight}) {
            for (bool label : {true, false}) for (bool loading : {true, false}) {
                OverlayConfig cfg;
                cfg.corner = corner; cfg.show_label = label; cfg.font_size = 64;
                const auto layout = compute_layout(cfg, scene.x, scene.y, loading);
                assert(layout.anchor.x >= 0 && layout.anchor.y >= 0);
                assert(layout.anchor.x+layout.width <= scene.x);
                assert(layout.anchor.y+layout.height <= scene.y);
                assert((layout.label_advance > 0) == label);
            }
        }
    }
    OverlayConfig cfg;
    const auto full = compute_layout(cfg, 1920,1080);
    const auto uhd = compute_layout(cfg, 3840,2160);
    assert(uhd.font_size == full.font_size*2);
    assert(uhd.anchor.y == full.anchor.y*2);
    const auto fallback = compute_layout(cfg, 0, std::numeric_limits<float>::quiet_NaN());
    assert(fallback.anchor.y == full.anchor.y);
    OverlayFrame f; f.config = *parse_settings_form(form); f.loading = false; f.fps = 60;
    auto p = make_wire_packet(f,42);
    const auto decoded = decode_wire_packet(p);
    assert(decoded && same_config(decoded->config, f.config));
    p.version = 1;
    assert(!decode_wire_packet(p)); // Old resident renderer must never misread v2.
    p.version = kWireVersion;
    p.size--;
    assert(!decode_wire_packet(p));
}

static void test_store_and_http(const std::string& dir) {
    const auto path = dir+"/config.ini";
    {
        SettingsStore store(path);
        status(settings_http_response(request("GET","/"),store,"<html>offline</html>"),200);
        status(settings_http_response(request("POST","/api/config",form),store,""),200);
        assert(!store.get().show_label && store.get().corner == Corner::TopRight);
        const auto before = store.get();
        status(settings_http_response(request("POST","/api/config",form,""),store,""),403);
        status(settings_http_response(request("POST","/api/config",form,
            "X-CommonFPS: 1\r\nOrigin: https://example.com\r\n"),store,""),403);
        status(settings_http_response(request("POST","/api/config",form+"&bad=1"),store,""),422);
        status(settings_http_response(request("POST","/api/config",form,
            "X-CommonFPS: 1\r\nContent-Length: 1\r\n"),store,""),400);
        status(settings_http_response(request("POST","/api/config",form,
            "X-CommonFPS: 1\r\nTransfer-Encoding: chunked\r\n"),store,""),400);
        auto wrong_host = request("GET", "/api/config");
        wrong_host.replace(wrong_host.find(kSettingsHost), std::string(kSettingsHost).size(), "example.com");
        status(settings_http_response(wrong_host,store,""),403);
        status(settings_http_response(request("GET","/../config.ini"),store,""),404);
        status(settings_http_response(std::string(4097,'x'),store,""),400);
        assert(same_config(store.get(), before));
        const auto reply = settings_http_response(request("GET","/api/config"),store,"");
        status(reply,200);
        assert(reply.find("value_color=#76F5AA") != std::string::npos);
    }
    {
        SettingsStore reloaded(path);
        assert(same_config(reloaded.get(), *parse_settings_form(form)));
    }
    SettingsStore unwritable(dir+"/missing/config.ini");
    assert(!unwritable.save(*parse_settings_form(form)));
    assert(same_config(unwritable.get(), default_config()));
    unlink(path.c_str());
    assert(symlink("/does/not/exist", (path+".tmp").c_str()) == 0);
    SettingsStore symlink_store(path);
    assert(!symlink_store.save(*parse_settings_form(form)));
    unlink((path+".tmp").c_str());
    // An oversized file never causes an unbounded read or partial settings load.
    { std::ofstream out(path); out << "font_size=64\n" << std::string(5000,'x'); }
    { SettingsStore oversized(path); assert(same_config(oversized.get(), default_config())); }
    unlink(path.c_str());
}

int main() {
    char dir[] = "/tmp/commonfps-settings-XXXXXX";
    assert(mkdtemp(dir));
    test_validation(); test_all_scenes_and_wire(); test_store_and_http(dir);
    rmdir(dir);
    std::cout << "Settings, persistence, HTTP, adaptive layout and wire tests passed\n";
}
