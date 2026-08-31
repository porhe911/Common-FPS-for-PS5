/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Source-only ShellUI renderer.
 *
 * The historical v1.0.0 binary embedded PHU's renderer.  v1.1.0 replaces
 * that blob with this small, auditable PUI implementation.  PUI mutations
 * are dispatched through Application.EnqueueEventAction(), which is the
 * UI-thread path proven on FW 9.60 during the SR9L..SR9O probes.
 */

#include "commonfps_shellui.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ps5/kernel.h>
#include <sys/socket.h>
#include <unistd.h>

namespace common_fps::ps5::shellui {

namespace {

/* Mono is intentionally represented by opaque types here. */
struct MonoDomain;
struct MonoThread;
struct MonoAssembly;
struct MonoImage;
struct MonoClass;
struct MonoMethod;
struct MonoProperty;
struct MonoObject;
struct MonoString;

using mono_get_root_domain_t = MonoDomain* (*)();
using mono_domain_get_t = MonoDomain* (*)();
using mono_thread_attach_t = MonoThread* (*)(MonoDomain*);
using mono_domain_assembly_open_t = MonoAssembly* (*)(MonoDomain*, const char*);
using mono_assembly_get_image_t = MonoImage* (*)(MonoAssembly*);
using mono_get_corlib_t = MonoImage* (*)();
using mono_class_from_name_t = MonoClass* (*)(MonoImage*, const char*, const char*);
using mono_class_get_method_from_name_t = MonoMethod* (*)(MonoClass*, const char*, int);
using mono_class_get_property_from_name_t = MonoProperty* (*)(MonoClass*, const char*);
using mono_property_get_get_method_t = MonoMethod* (*)(MonoProperty*);
using mono_property_get_set_method_t = MonoMethod* (*)(MonoProperty*);
using mono_runtime_invoke_t = MonoObject* (*)(MonoMethod*, void*, void**, MonoObject**);
using mono_string_new_t = MonoString* (*)(MonoDomain*, const char*);
using mono_object_new_t = MonoObject* (*)(MonoDomain*, MonoClass*);
using mono_runtime_object_init_t = void (*)(MonoObject*);
using mono_object_unbox_t = void* (*)(MonoObject*);
using mono_compile_method_t = void* (*)(MonoMethod*);

mono_get_root_domain_t mono_get_root_domain_{};
mono_domain_get_t mono_domain_get_{};
mono_thread_attach_t mono_thread_attach_{};
mono_domain_assembly_open_t mono_domain_assembly_open_{};
mono_assembly_get_image_t mono_assembly_get_image_{};
mono_get_corlib_t mono_get_corlib_{};
mono_class_from_name_t mono_class_from_name_{};
mono_class_get_method_from_name_t mono_class_get_method_from_name_{};
mono_class_get_property_from_name_t mono_class_get_property_from_name_{};
mono_property_get_get_method_t mono_property_get_get_method_{};
mono_property_get_set_method_t mono_property_get_set_method_{};
mono_runtime_invoke_t mono_runtime_invoke_{};
mono_string_new_t mono_string_new_{};
mono_object_new_t mono_object_new_{};
mono_runtime_object_init_t mono_runtime_object_init_{};
mono_object_unbox_t mono_object_unbox_{};
mono_compile_method_t mono_compile_method_{};

MonoDomain* g_domain{};
MonoImage* g_pui_image{};
MonoObject* g_game_scene{};
MonoObject* g_application{};
MonoMethod* g_enqueue_event_action{};
MonoClass* g_action_class{};
MonoMethod* g_action_ctor{};

std::atomic_bool g_runtime_ready{false};
std::atomic_bool g_running{false};
std::atomic_bool g_have_packet{false};
std::atomic_bool g_action_pending{false};
std::atomic<std::uint64_t> g_sequence{0};
std::atomic<std::uint64_t> g_queued_sequence{0};

pthread_t g_receiver_thread{};
WirePacket g_packet{};
WirePacket g_pending_packet{};
pthread_mutex_t g_packet_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_pending_lock = PTHREAD_MUTEX_INITIALIZER;

constexpr const char* kLabelId = "id_commonfps_label";
constexpr const char* kValueId = "id_commonfps_value";
constexpr const char* kPuiDll =
    "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll";
constexpr const char* kAppSystemDll =
    "/system_ex/common_ex/lib/Sce.Vsh.ShellUI.AppSystem.dll";

void log_line(const char* fmt, ...) {
    FILE* fp = std::fopen("/data/CommonFPS_v110_shellui.log", "a");
    if (!fp)
        return;

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fclose(fp);
}

template <typename T>
bool resolve_symbol(std::uint32_t handle, const char* name, T& out) {
    out = reinterpret_cast<T>(
        kernel_dynlib_dlsym(-1, handle, name));
    return out != nullptr;
}

MonoImage* open_image(const char* path) {
    if (!g_domain || !mono_domain_assembly_open_ || !mono_assembly_get_image_)
        return nullptr;

    MonoAssembly* assembly = mono_domain_assembly_open_(g_domain, path);
    return assembly ? mono_assembly_get_image_(assembly) : nullptr;
}

MonoObject* invoke(
    MonoMethod* method,
    MonoObject* instance,
    void** args = nullptr) {

    if (!method || !mono_runtime_invoke_)
        return nullptr;

    MonoObject* exception = nullptr;
    MonoObject* result = mono_runtime_invoke_(
        method,
        instance,
        args,
        &exception);

    return exception ? nullptr : result;
}

MonoMethod* property_getter(MonoClass* klass, const char* name) {
    if (!klass)
        return nullptr;
    MonoProperty* property =
        mono_class_get_property_from_name_(klass, name);
    return property ? mono_property_get_get_method_(property) : nullptr;
}

MonoMethod* property_setter(MonoClass* klass, const char* name) {
    if (!klass)
        return nullptr;
    MonoProperty* property =
        mono_class_get_property_from_name_(klass, name);
    return property ? mono_property_get_set_method_(property) : nullptr;
}

template <typename T>
bool set_property_direct(
    MonoClass* klass,
    MonoObject* instance,
    const char* name,
    T value) {

    MonoMethod* setter = property_setter(klass, name);
    if (!setter)
        return false;

    void* thunk = mono_compile_method_(setter);
    if (!thunk)
        return false;

    auto fn = reinterpret_cast<void (*)(MonoObject*, T)>(thunk);
    fn(instance, value);
    return true;
}

bool set_property_object(
    MonoClass* klass,
    MonoObject* instance,
    const char* name,
    MonoObject* value) {

    MonoMethod* setter = property_setter(klass, name);
    if (!setter)
        return false;

    void* args[1] = {value};
    MonoObject* exception = nullptr;
    mono_runtime_invoke_(setter, instance, args, &exception);
    return exception == nullptr;
}

MonoObject* get_root_widget() {
    MonoClass* scene = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Scene");
    MonoMethod* getter = property_getter(scene, "RootWidget");
    return getter ? invoke(getter, g_game_scene) : nullptr;
}

MonoObject* create_font(int size) {
    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "UIFont");
    if (!klass)
        return nullptr;

    MonoObject* boxed = mono_object_new_(g_domain, klass);
    if (!boxed)
        return nullptr;

    void* real = mono_object_unbox_(boxed);
    MonoMethod* ctor = mono_class_get_method_from_name_(klass, ".ctor", 3);
    void* thunk = ctor ? mono_compile_method_(ctor) : nullptr;
    if (!real || !thunk)
        return nullptr;

    auto fn = reinterpret_cast<void (*)(void*, int, int, int)>(thunk);
    fn(real, size, 0, 0);
    return reinterpret_cast<MonoObject*>(real);
}

MonoObject* create_color(float r, float g, float b, float a) {
    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI",
        "UIColor");
    if (!klass)
        return nullptr;

    MonoObject* boxed = mono_object_new_(g_domain, klass);
    if (!boxed)
        return nullptr;

    void* real = mono_object_unbox_(boxed);
    MonoMethod* ctor = mono_class_get_method_from_name_(klass, ".ctor", 4);
    void* thunk = ctor ? mono_compile_method_(ctor) : nullptr;
    if (!real || !thunk)
        return nullptr;

    auto fn = reinterpret_cast<void (*)(void*, float, float, float, float)>(thunk);
    fn(real, r, g, b, a);
    return reinterpret_cast<MonoObject*>(real);
}

MonoObject* create_label(
    const char* name,
    float x,
    float y,
    const char* text,
    MonoObject* font,
    int horizontal_alignment,
    float r,
    float g,
    float b,
    float a) {

    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Label");
    if (!klass)
        return nullptr;

    MonoObject* label = mono_object_new_(g_domain, klass);
    if (!label)
        return nullptr;

    mono_runtime_object_init_(label);

    MonoString* mono_name = mono_string_new_(g_domain, name);
    MonoString* mono_text = mono_string_new_(g_domain, text);
    MonoObject* color = create_color(r, g, b, a);

    bool ok = true;
    ok &= set_property_direct(klass, label, "Name", mono_name);
    ok &= set_property_direct(klass, label, "X", x);
    ok &= set_property_direct(klass, label, "Y", y);
    ok &= set_property_direct(klass, label, "Text", mono_text);
    ok &= set_property_object(klass, label, "Font", font);
    ok &= set_property_direct(
        klass, label, "HorizontalAlignment", horizontal_alignment);
    ok &= set_property_direct(klass, label, "VerticalAlignment", 0);
    ok &= set_property_object(klass, label, "TextColor", color);
    ok &= set_property_direct(klass, label, "FitWidthToText", true);
    ok &= set_property_direct(klass, label, "FitHeightToText", true);

    return ok ? label : nullptr;
}

MonoObject* find_widget(MonoObject* root, const char* id) {
    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Widget");
    MonoMethod* method = klass
        ? mono_class_get_method_from_name_(klass, "FindWidgetByName", 1)
        : nullptr;
    void* thunk = method ? mono_compile_method_(method) : nullptr;
    if (!root || !thunk)
        return nullptr;

    MonoString* name = mono_string_new_(g_domain, id);
    auto fn = reinterpret_cast<MonoObject* (*)(MonoObject*, MonoString*)>(thunk);
    return fn(root, name);
}

bool append_child(MonoObject* root, MonoObject* child) {
    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Widget");
    MonoMethod* method = klass
        ? mono_class_get_method_from_name_(klass, "AppendChild", 1)
        : nullptr;
    if (!method || !root || !child)
        return false;

    void* args[1] = {child};
    MonoObject* exception = nullptr;
    mono_runtime_invoke_(method, root, args, &exception);
    return exception == nullptr;
}

bool set_label_text(MonoObject* label, const char* text) {
    if (!label)
        return false;

    MonoClass* klass = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Label");
    MonoString* mono_text = mono_string_new_(g_domain, text);
    return set_property_direct(klass, label, "Text", mono_text);
}

void ui_apply_callback() {
    WirePacket packet{};
    pthread_mutex_lock(&g_pending_lock);
    packet = g_pending_packet;
    pthread_mutex_unlock(&g_pending_lock);

    const auto decoded = decode_wire_packet(packet);
    if (!decoded) {
        g_action_pending.store(false);
        return;
    }

    const OverlayFrame& frame = *decoded;
    MonoObject* root = get_root_widget();
    if (!root) {
        log_line("UI root unavailable seq=%llu",
                 static_cast<unsigned long long>(packet.sequence));
        g_action_pending.store(false);
        return;
    }

    MonoObject* label = find_widget(root, kLabelId);
    MonoObject* value = find_widget(root, kValueId);

    if (!label || !value) {
        MonoObject* font = create_font(frame.config.font_size);
        if (!font) {
            log_line("UIFont create failed");
            g_action_pending.store(false);
            return;
        }

        const float x = frame.anchor.x;
        const float y = frame.anchor.y;
        const float value_x =
            x + static_cast<float>(frame.config.font_size) * 2.7f;

        if (!label) {
            label = create_label(
                kLabelId,
                x,
                y,
                "FPS:",
                font,
                1,
                0.702f,
                0.400f,
                1.000f,
                1.000f);
            if (label)
                append_child(root, label);
        }

        if (!value) {
            value = create_label(
                kValueId,
                value_x,
                y,
                "Loading",
                font,
                0,
                1.0f,
                1.0f,
                1.0f,
                1.0f);
            if (value)
                append_child(root, value);
        }

        log_line(
            "widgets create seq=%llu label=%p value=%p font=%d x=%.1f y=%.1f",
            static_cast<unsigned long long>(packet.sequence),
            static_cast<void*>(label),
            static_cast<void*>(value),
            frame.config.font_size,
            x,
            y);
    }

    if (value) {
        char text[32]{};
        if (frame.loading)
            std::snprintf(text, sizeof(text), "Loading");
        else
            std::snprintf(text, sizeof(text), "%d", frame.fps);
        set_label_text(value, text);
    }

    g_action_pending.store(false);
}

MonoObject* make_native_action() {
    MonoObject* action = mono_object_new_(g_domain, g_action_class);
    if (!action)
        return nullptr;

    void* callback = reinterpret_cast<void*>(&ui_apply_callback);
    void* args[2] = {nullptr, &callback};
    MonoObject* exception = nullptr;
    mono_runtime_invoke_(g_action_ctor, action, args, &exception);
    return exception ? nullptr : action;
}

bool enqueue_ui_action(MonoObject* root) {
    MonoObject* action = make_native_action();
    if (!action)
        return false;

    void* args[2] = {root, action};
    MonoObject* exception = nullptr;
    mono_runtime_invoke_(
        g_enqueue_event_action,
        g_application,
        args,
        &exception);
    return exception == nullptr;
}

void* receiver_thread_main(void*) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        log_line("receiver socket failed");
        return nullptr;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultIpcPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_line("receiver bind failed port=%u", kDefaultIpcPort);
        close(fd);
        return nullptr;
    }

    log_line("receiver ready port=%u", kDefaultIpcPort);

    while (g_running.load()) {
        WirePacket packet{};
        const ssize_t n = recv(fd, &packet, sizeof(packet), 0);
        if (n != static_cast<ssize_t>(sizeof(packet)))
            continue;

        if (packet.magic != kWireMagic ||
            packet.version != kWireVersion ||
            packet.size != sizeof(WirePacket)) {
            continue;
        }

        pthread_mutex_lock(&g_packet_lock);
        g_packet = packet;
        pthread_mutex_unlock(&g_packet_lock);

        g_sequence.store(packet.sequence);
        g_have_packet.store(true);
    }

    close(fd);
    return nullptr;
}

} // namespace

bool initialize_runtime() {
    if (g_runtime_ready.load())
        return true;

    std::uint32_t mono_handle = 0;
    if (kernel_dynlib_handle(
            getpid(),
            "libmonosgen-2.0.sprx",
            &mono_handle) < 0) {
        log_line("libmono handle failed");
        return false;
    }

    bool ok = true;
    ok &= resolve_symbol(mono_handle, "mono_get_root_domain", mono_get_root_domain_);
    ok &= resolve_symbol(mono_handle, "mono_domain_get", mono_domain_get_);
    ok &= resolve_symbol(mono_handle, "mono_thread_attach", mono_thread_attach_);
    ok &= resolve_symbol(mono_handle, "mono_domain_assembly_open", mono_domain_assembly_open_);
    ok &= resolve_symbol(mono_handle, "mono_assembly_get_image", mono_assembly_get_image_);
    ok &= resolve_symbol(mono_handle, "mono_get_corlib", mono_get_corlib_);
    ok &= resolve_symbol(mono_handle, "mono_class_from_name", mono_class_from_name_);
    ok &= resolve_symbol(
        mono_handle,
        "mono_class_get_method_from_name",
        mono_class_get_method_from_name_);
    ok &= resolve_symbol(
        mono_handle,
        "mono_class_get_property_from_name",
        mono_class_get_property_from_name_);
    ok &= resolve_symbol(
        mono_handle,
        "mono_property_get_get_method",
        mono_property_get_get_method_);
    ok &= resolve_symbol(
        mono_handle,
        "mono_property_get_set_method",
        mono_property_get_set_method_);
    ok &= resolve_symbol(mono_handle, "mono_runtime_invoke", mono_runtime_invoke_);
    ok &= resolve_symbol(mono_handle, "mono_string_new", mono_string_new_);
    ok &= resolve_symbol(mono_handle, "mono_object_new", mono_object_new_);
    ok &= resolve_symbol(
        mono_handle,
        "mono_runtime_object_init",
        mono_runtime_object_init_);
    ok &= resolve_symbol(mono_handle, "mono_object_unbox", mono_object_unbox_);
    ok &= resolve_symbol(mono_handle, "mono_compile_method", mono_compile_method_);

    if (!ok) {
        log_line("mono symbol resolution failed");
        return false;
    }

    g_domain = mono_get_root_domain_();
    if (!g_domain) {
        log_line("root domain failed");
        return false;
    }
    mono_thread_attach_(g_domain);

    g_pui_image = open_image(kPuiDll);
    MonoImage* app_system = open_image(kAppSystemDll);
    if (!g_pui_image || !app_system) {
        log_line("managed image open failed pui=%p app=%p",
                 static_cast<void*>(g_pui_image),
                 static_cast<void*>(app_system));
        return false;
    }

    MonoClass* layer_manager = mono_class_from_name_(
        app_system,
        "Sce.Vsh.ShellUI.AppSystem",
        "LayerManager");
    MonoMethod* find_scene = layer_manager
        ? mono_class_get_method_from_name_(
              layer_manager,
              "FindContainerSceneByPath",
              1)
        : nullptr;
    if (!find_scene) {
        log_line("FindContainerSceneByPath unavailable");
        return false;
    }

    MonoDomain* active_domain = mono_domain_get_();
    MonoString* game_path = mono_string_new_(
        active_domain ? active_domain : g_domain,
        "Game");
    void* find_args[1] = {game_path};
    g_game_scene = invoke(find_scene, nullptr, find_args);
    if (!g_game_scene) {
        log_line("Game ContainerScene unavailable");
        return false;
    }

    MonoClass* application_class = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI",
        "Application");
    MonoMethod* get_instance = application_class
        ? mono_class_get_method_from_name_(application_class, "get_Instance", 0)
        : nullptr;
    g_enqueue_event_action = application_class
        ? mono_class_get_method_from_name_(
              application_class,
              "EnqueueEventAction",
              2)
        : nullptr;
    g_application = get_instance ? invoke(get_instance, nullptr) : nullptr;

    MonoImage* corlib = mono_get_corlib_();
    g_action_class = corlib
        ? mono_class_from_name_(corlib, "System", "Action")
        : nullptr;
    g_action_ctor = g_action_class
        ? mono_class_get_method_from_name_(g_action_class, ".ctor", 2)
        : nullptr;

    if (!g_application || !g_enqueue_event_action || !g_action_ctor) {
        log_line(
            "UI queue unavailable app=%p enqueue=%p action_ctor=%p",
            static_cast<void*>(g_application),
            static_cast<void*>(g_enqueue_event_action),
            static_cast<void*>(g_action_ctor));
        return false;
    }

    g_runtime_ready.store(true);
    log_line("runtime ready pid=%d", getpid());
    return true;
}

bool initialize_receiver() {
    if (g_running.exchange(true))
        return true;

    if (pthread_create(
            &g_receiver_thread,
            nullptr,
            receiver_thread_main,
            nullptr) != 0) {
        g_running.store(false);
        log_line("receiver pthread_create failed");
        return false;
    }

    pthread_detach(g_receiver_thread);
    return true;
}

void shutdown_receiver() {
    g_running.store(false);
}

void apply_latest_state() {
    if (!g_runtime_ready.load() ||
        !g_have_packet.load() ||
        g_action_pending.load()) {
        return;
    }

    const std::uint64_t sequence = g_sequence.load();
    if (sequence == g_queued_sequence.load())
        return;

    WirePacket packet{};
    pthread_mutex_lock(&g_packet_lock);
    packet = g_packet;
    pthread_mutex_unlock(&g_packet_lock);

    if (!decode_wire_packet(packet))
        return;

    MonoObject* root = get_root_widget();
    if (!root)
        return;

    pthread_mutex_lock(&g_pending_lock);
    g_pending_packet = packet;
    pthread_mutex_unlock(&g_pending_lock);

    g_action_pending.store(true);
    if (!enqueue_ui_action(root)) {
        g_action_pending.store(false);
        log_line("EnqueueEventAction failed seq=%llu",
                 static_cast<unsigned long long>(sequence));
        return;
    }

    g_queued_sequence.store(sequence);
}

} // namespace common_fps::ps5::shellui
