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
 * that blob with this small, auditable PUI implementation. PARITY TEST13
 * leaves the TEST12 shared-ELF/Application.Update, pinned-GC-handle and
 * two-second socket receive-timeout behavior unchanged while the controller
 * isolates the bootstrap-stack parity difference.
 */

#include "commonfps_shellui.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace common_fps::ps5::shellui {

namespace {

static_assert(SOL_SOCKET == 0xffff);
static_assert(SO_RCVTIMEO == 0x1006);
static_assert(sizeof(timeval) == 16);

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
using mono_gchandle_new_t = std::uint32_t (*)(MonoObject*, int);
using mono_gchandle_get_target_t = MonoObject* (*)(std::uint32_t);
using mono_gchandle_free_t = void (*)(std::uint32_t);

/*
 * The injector resolves these imports against ShellUI's already-loaded Mono
 * module before the remote thread starts.  This deliberately avoids linking
 * the PS5 payload CRT merely to call kernel_dynlib_handle/dlsym from inside
 * ShellUI.  The stable v1.0.0 renderer used the same direct-import shape.
 */
extern "C" {
MonoDomain* mono_get_root_domain();
MonoDomain* mono_domain_get();
MonoThread* mono_thread_attach(MonoDomain*);
MonoAssembly* mono_domain_assembly_open(MonoDomain*, const char*);
MonoImage* mono_assembly_get_image(MonoAssembly*);
MonoClass* mono_class_from_name(MonoImage*, const char*, const char*);
MonoMethod* mono_class_get_method_from_name(MonoClass*, const char*, int);
MonoProperty* mono_class_get_property_from_name(MonoClass*, const char*);
MonoMethod* mono_property_get_get_method(MonoProperty*);
MonoMethod* mono_property_get_set_method(MonoProperty*);
MonoObject* mono_runtime_invoke(MonoMethod*, void*, void**, MonoObject**);
MonoString* mono_string_new(MonoDomain*, const char*);
MonoObject* mono_object_new(MonoDomain*, MonoClass*);
void mono_runtime_object_init(MonoObject*);
void* mono_object_unbox(MonoObject*);
void* mono_compile_method(MonoMethod*);
std::uint32_t mono_gchandle_new(MonoObject*, int);
MonoObject* mono_gchandle_get_target(std::uint32_t);
void mono_gchandle_free(std::uint32_t);
}

mono_get_root_domain_t mono_get_root_domain_ = mono_get_root_domain;
mono_domain_get_t mono_domain_get_ = mono_domain_get;
mono_thread_attach_t mono_thread_attach_ = mono_thread_attach;
mono_domain_assembly_open_t mono_domain_assembly_open_ =
    mono_domain_assembly_open;
mono_assembly_get_image_t mono_assembly_get_image_ = mono_assembly_get_image;
mono_class_from_name_t mono_class_from_name_ = mono_class_from_name;
mono_class_get_method_from_name_t mono_class_get_method_from_name_ =
    mono_class_get_method_from_name;
mono_class_get_property_from_name_t mono_class_get_property_from_name_ =
    mono_class_get_property_from_name;
mono_property_get_get_method_t mono_property_get_get_method_ =
    mono_property_get_get_method;
mono_property_get_set_method_t mono_property_get_set_method_ =
    mono_property_get_set_method;
mono_runtime_invoke_t mono_runtime_invoke_ = mono_runtime_invoke;
mono_string_new_t mono_string_new_ = mono_string_new;
mono_object_new_t mono_object_new_ = mono_object_new;
mono_runtime_object_init_t mono_runtime_object_init_ =
    mono_runtime_object_init;
mono_object_unbox_t mono_object_unbox_ = mono_object_unbox;
mono_compile_method_t mono_compile_method_ = mono_compile_method;
mono_gchandle_new_t mono_gchandle_new_ = mono_gchandle_new;
mono_gchandle_get_target_t mono_gchandle_get_target_ =
    mono_gchandle_get_target;
mono_gchandle_free_t mono_gchandle_free_ = mono_gchandle_free;

MonoDomain* g_domain{};
MonoImage* g_pui_image{};
std::uint32_t g_game_scene_handle{};
std::uint32_t g_label_handle{};
std::uint32_t g_value_handle{};

using application_update_t = void (*)(MonoObject*);
application_update_t g_application_update_original{};

std::atomic_bool g_runtime_ready{false};
std::atomic_bool g_have_packet{false};
std::atomic<std::uint64_t> g_sequence{0};
std::atomic<std::uint64_t> g_applied_sequence{0};

int g_receiver_fd = -1;
WirePacket g_packet{};
pthread_mutex_t g_packet_lock = PTHREAD_MUTEX_INITIALIZER;

constexpr const char* kLabelId = "id_commonfps_label";
constexpr const char* kValueId = "id_commonfps_value";
constexpr const char* kPuiDll =
    "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll";
constexpr const char* kAppSystemDll =
    "/system_ex/common_ex/lib/Sce.Vsh.ShellUI.AppSystem.dll";

/*
 * etaHEN installs an x86-64 absolute-indirect jump at Application.Update:
 *
 *   FF 25 00 00 00 00 <eight-byte destination>
 *
 * TEST13 only chains that already-proven hook. It never disassembles or
 * overwrites an unhooked Sony method. The trampoline lives in this payload's
 * RWE text segment and receives an exact copy of etaHEN's 14-byte jump.
 */
extern "C" __attribute__((naked, noinline, used, aligned(16)))
void commonfps_update_trampoline() {
    __asm__ volatile(
        ".rept 32\n"
        "nop\n"
        ".endr\n"
        "ret\n");
}

void application_update_hook(MonoObject* instance);

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

MonoObject* managed_target(std::uint32_t handle) {
    return handle != 0 && mono_gchandle_get_target_
        ? mono_gchandle_get_target_(handle)
        : nullptr;
}

bool replace_managed_handle(
    std::uint32_t& handle,
    MonoObject* object) {

    if (!object || !mono_gchandle_new_ || !mono_gchandle_get_target_)
        return false;

    if (handle != 0 && managed_target(handle) == object)
        return true;

    if (handle != 0 && mono_gchandle_free_)
        mono_gchandle_free_(handle);

    /* Match the hardware-stable v1.0.0 renderer: retain a pinned object. */
    handle = mono_gchandle_new_(object, 1);
    return handle != 0 && managed_target(handle) == object;
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
    MonoObject* game_scene = managed_target(g_game_scene_handle);
    if (!game_scene)
        return nullptr;

    MonoClass* scene = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI.UI2",
        "Scene");
    MonoMethod* getter = property_getter(scene, "RootWidget");
    return getter ? invoke(getter, game_scene) : nullptr;
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

bool apply_packet_on_render(const WirePacket& packet) {
    const auto decoded = decode_wire_packet(packet);
    if (!decoded)
        return false;

    const OverlayFrame& frame = *decoded;
    MonoObject* root = get_root_widget();
    if (!root) {
        static std::uint64_t last_logged_sequence = 0;
        if (last_logged_sequence != packet.sequence) {
            last_logged_sequence = packet.sequence;
            log_line("UI root unavailable seq=%llu",
                     static_cast<unsigned long long>(packet.sequence));
        }
        return false;
    }

    MonoObject* label = managed_target(g_label_handle);
    MonoObject* value = managed_target(g_value_handle);

    if (!label)
        label = find_widget(root, kLabelId);
    if (!value)
        value = find_widget(root, kValueId);

    if (!label || !value) {
        MonoObject* font = create_font(frame.config.font_size);
        if (!font) {
            log_line("UIFont create failed");
            return false;
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
            if (label && !append_child(root, label))
                label = nullptr;
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
            if (value && !append_child(root, value))
                value = nullptr;
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

    if (!label || !value)
        return false;

    if (!replace_managed_handle(g_label_handle, label) ||
        !replace_managed_handle(g_value_handle, value)) {
        log_line(
            "widget GC handle failed label=%u value=%u",
            g_label_handle,
            g_value_handle);
        return false;
    }

    char text[32]{};
    if (frame.loading)
        std::snprintf(text, sizeof(text), "Loading");
    else
        std::snprintf(text, sizeof(text), "%d", frame.fps);
    return set_label_text(value, text);
}

void apply_latest_state_on_render() {
    if (!g_runtime_ready.load() || !g_have_packet.load())
        return;

    const std::uint64_t sequence = g_sequence.load();
    if (sequence == g_applied_sequence.load())
        return;

    WirePacket packet{};
    pthread_mutex_lock(&g_packet_lock);
    packet = g_packet;
    pthread_mutex_unlock(&g_packet_lock);

    if (packet.sequence != sequence || !decode_wire_packet(packet))
        return;

    if (apply_packet_on_render(packet))
        g_applied_sequence.store(packet.sequence);
}

void application_update_hook(MonoObject* instance) {
    apply_latest_state_on_render();

    if (g_application_update_original)
        g_application_update_original(instance);
}

bool install_etahen_update_hook(MonoClass* application_class) {
    MonoMethod* update = application_class
        ? mono_class_get_method_from_name_(
              application_class,
              "Update",
              0)
        : nullptr;
    auto* address = update
        ? static_cast<std::uint8_t*>(mono_compile_method_(update))
        : nullptr;
    if (!address) {
        log_line("Application.Update compile failed");
        return false;
    }

    static constexpr std::uint8_t kEtaHenJumpPrefix[6] = {
        0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
    };
    if (std::memcmp(
            address,
            kEtaHenJumpPrefix,
            sizeof(kEtaHenJumpPrefix)) != 0) {
        log_line(
            "Application.Update etaHEN hook unavailable bytes=%02x%02x%02x%02x%02x%02x",
            address[0],
            address[1],
            address[2],
            address[3],
            address[4],
            address[5]);
        return false;
    }

    constexpr std::size_t kJumpSize = 14;
    auto* trampoline = reinterpret_cast<std::uint8_t*>(
        &commonfps_update_trampoline);
    std::memcpy(trampoline, address, kJumpSize);
    __builtin___clear_cache(
        reinterpret_cast<char*>(trampoline),
        reinterpret_cast<char*>(trampoline + kJumpSize));

    std::uint64_t previous_destination = 0;
    std::memcpy(
        &previous_destination,
        address + sizeof(kEtaHenJumpPrefix),
        sizeof(previous_destination));

    g_application_update_original =
        reinterpret_cast<application_update_t>(trampoline);

    const std::uint64_t new_destination =
        reinterpret_cast<std::uint64_t>(&application_update_hook);
    std::memcpy(
        address + sizeof(kEtaHenJumpPrefix),
        &new_destination,
        sizeof(new_destination));
    __builtin___clear_cache(
        reinterpret_cast<char*>(address),
        reinterpret_cast<char*>(address + kJumpSize));

    log_line(
        "Application.Update chain online method=%p previous=%p hook=%p",
        static_cast<void*>(address),
        reinterpret_cast<void*>(previous_destination),
        reinterpret_cast<void*>(new_destination));
    return true;
}

} // namespace

bool initialize_runtime() {
    if (g_runtime_ready.load())
        return true;

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
    MonoObject* game_scene = invoke(find_scene, nullptr, find_args);
    if (!game_scene) {
        log_line("Game ContainerScene unavailable");
        return false;
    }
    if (!replace_managed_handle(g_game_scene_handle, game_scene)) {
        log_line("Game ContainerScene GC handle failed");
        return false;
    }

    MonoClass* application_class = mono_class_from_name_(
        g_pui_image,
        "Sce.PlayStation.PUI",
        "Application");
    if (!application_class || !install_etahen_update_hook(application_class)) {
        log_line("etaHEN Application.Update chain unavailable");
        return false;
    }

    g_runtime_ready.store(true);
    log_line(
        "runtime ready pid=%d scene_handle=%u gc_mode=pinned",
        getpid(),
        g_game_scene_handle);
    return true;
}

bool initialize_receiver() {
    if (g_receiver_fd >= 0)
        return true;

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        log_line("receiver socket failed");
        return false;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    /*
     * The hardware-stable v1.0.0 renderer used SO_RCVTIMEO with the exact
     * timeval {2, 0}.  This lets the render hook stop consuming stale state
     * after the controller disappears during process teardown while keeping
     * the injected receiver thread resident.
     */
    timeval receive_timeout{};
    receive_timeout.tv_sec = 2;
    receive_timeout.tv_usec = 0;
    if (setsockopt(
            fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &receive_timeout,
            sizeof(receive_timeout)) < 0) {
        log_line("receiver timeout setup failed");
        close(fd);
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDefaultIpcPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(
            fd,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)) < 0) {
        log_line("receiver bind failed port=%u", kDefaultIpcPort);
        close(fd);
        return false;
    }

    g_receiver_fd = fd;
    log_line(
        "receiver ready port=%u mode=persistent-main-thread "
        "timeout_ms=2000 stale=1",
        kDefaultIpcPort);
    return true;
}

[[noreturn]] void run_receiver_loop() {
    for (;;) {
        WirePacket packet{};
        const ssize_t n = recv(
            g_receiver_fd,
            &packet,
            sizeof(packet),
            0);
        if (n != static_cast<ssize_t>(sizeof(packet))) {
            if (n < 0) {
                /*
                 * Match v1.0.0's timeout behavior: invalidate the last
                 * controller state, but never close, unhook or return.
                 * A later valid packet makes the renderer live again.
                 */
                g_have_packet.store(false);
                usleep(10000);
            }
            continue;
        }

        /*
         * TEST8 proved that returning the injected renderer thread can KP.
         * A stale shutdown packet is deliberately ignored in TEST13.
         */
        if (is_shutdown_wire_packet(packet))
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
}

} // namespace common_fps::ps5::shellui
