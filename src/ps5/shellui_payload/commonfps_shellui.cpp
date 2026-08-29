/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "HookedFuncs.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

namespace common_fps::ps5::shellui {

namespace {

std::atomic_bool g_running{false};
std::atomic_bool g_have_packet{false};
std::atomic<std::uint64_t> g_sequence{0};
std::atomic_bool g_logged_packet{false};

pthread_t g_thread{};
WirePacket g_packet{};
pthread_mutex_t g_packet_lock = PTHREAD_MUTEX_INITIALIZER;

constexpr const char* kLabelId = "id_commonfps_label";
constexpr const char* kValueId = "id_commonfps_value";

void renderer_log(const char* format, const char* value = nullptr) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_shellui.log", "a");
    if (!f)
        return;
    if (value)
        std::fprintf(f, format, value);
    else
        std::fprintf(f, "%s", format);
    std::fprintf(f, "\n");
    std::fclose(f);
}

void remove_widget(MonoObject* root, const char* id) {
    MonoClass* widgetClass = mono_class_from_name(
        pui_img,
        "Sce.PlayStation.PUI.UI2",
        "Widget");
    if (!widgetClass)
        return;

    MonoObject* child = Invoke<MonoObject*>(
        pui_img,
        widgetClass,
        root,
        "FindWidgetByName",
        mono_string_new(Root_Domain, id));

    if (child) {
        Invoke<void>(
            pui_img,
            widgetClass,
            child,
            "RemoveFromParent");
    }
}

void* receiver_thread(void*) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return nullptr;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultIpcPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        renderer_log("R6 FAIL state receiver bind");
        close(fd);
        return nullptr;
    }

    while (g_running.load()) {
        WirePacket packet{};
        const ssize_t n = recv(fd, &packet, sizeof(packet), 0);

        if (n != static_cast<ssize_t>(sizeof(packet)))
            continue;
        if (packet.magic != kWireMagic ||
            packet.version != kWireVersion ||
            packet.size != sizeof(WirePacket))
            continue;

        pthread_mutex_lock(&g_packet_lock);
        g_packet = packet;
        pthread_mutex_unlock(&g_packet_lock);

        g_sequence.store(packet.sequence);
        g_have_packet.store(true);

        if (!g_logged_packet.exchange(true))
            renderer_log("R6 first valid state packet received");
    }

    close(fd);
    return nullptr;
}

} // namespace

bool initialize_receiver() {
    if (g_running.exchange(true))
        return true;

    if (pthread_create(&g_thread, nullptr, receiver_thread, nullptr) != 0) {
        g_running.store(false);
        return false;
    }

    pthread_detach(g_thread);
    return true;
}

void shutdown_receiver() {
    g_running.store(false);
}

void apply_latest_state() {
    static std::uint64_t applied_sequence = 0;
    static bool logged_widget_append = false;

    if (!g_have_packet.load())
        return;

    const std::uint64_t sequence = g_sequence.load();
    if (sequence == applied_sequence)
        return;

    WirePacket packet{};
    pthread_mutex_lock(&g_packet_lock);
    packet = g_packet;
    pthread_mutex_unlock(&g_packet_lock);

    const auto decoded = decode_wire_packet(packet);
    if (!decoded)
        return;

    const OverlayFrame& frame = *decoded;

    MonoObject* root = Get_Property<MonoObject*>(
        pui_img,
        "Sce.PlayStation.PUI.UI2",
        "Scene",
        Game,
        "RootWidget");
    if (!root)
        return;

    remove_widget(root, kLabelId);
    remove_widget(root, kValueId);

    MonoObject* font = CreateUIFont(frame.config.font_size, 0, 0);
    if (!font)
        return;

    const float x = frame.anchor.x;
    const float y = frame.anchor.y;

    MonoObject* label = CreateLabel(
        kLabelId,
        x,
        y,
        "FPS:",
        font,
        1,
        0,
        0.702f,
        0.400f,
        1.000f,
        1.000f);

    char value_text[32]{};
    if (frame.loading)
        std::snprintf(value_text, sizeof(value_text), "loading");
    else
        std::snprintf(value_text, sizeof(value_text), "%d", frame.fps);

    const float value_x =
        x + static_cast<float>(frame.config.font_size) * 2.7f;

    MonoObject* value = CreateLabel(
        kValueId,
        value_x,
        y,
        value_text,
        font,
        0,
        0,
        1.0f,
        1.0f,
        1.0f,
        1.0f);

    if (!label || !value)
        return;

    Widget_Append_Child(root, label);
    Widget_Append_Child(root, value);

    if (!logged_widget_append) {
        renderer_log("R7 widgets appended; value=%s", value_text);
        logged_widget_append = true;
    }

    applied_sequence = sequence;
}

} // namespace common_fps::ps5::shellui
