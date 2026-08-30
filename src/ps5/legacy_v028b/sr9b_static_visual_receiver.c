/*
 * Common FPS v0.28b SR9B - static PUI visual ShellUI receiver
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Builds directly on hardware-proven SR9A. It resolves the same Mono/PUI
 * context chain, creates exactly two static PUI labels once, then keeps the
 * proven PHUF receiver/health ACK lifecycle unchanged. No Detour, no hook,
 * no per-frame UI mutation, and no dynamic FPS text update in this stage.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PHUF_MAGIC 0x46554850u
#define PHUF_VERSION 1u
#define PHUF_PORT_NETWORK 0xF5D8u
#define HEALTH_PORT_NETWORK 0xF6D8u
#define LOOPBACK_ADDRESS 0x0100007fu
#define PHUF_TEXT_SIZE 1024u
#define HEALTH_MAGIC 0x42394852u /* "RH9B" in memory */
#define HEALTH_READY 1u
#define HEALTH_PACKET 2u
#define HEALTH_CONTEXT 3u
#define HEALTH_VISUAL 4u
#define CONTEXT_FULL_MASK 0x7fffu
#define VISUAL_READY_STAGE 5u

typedef void MonoDomain;
typedef void MonoThread;
typedef void MonoAssembly;
typedef void MonoImage;
typedef void MonoClass;
typedef void MonoMethod;
typedef void MonoString;
typedef void MonoObject;
typedef void MonoProperty;

typedef MonoDomain* (*mono_get_root_domain_fn)(void);
typedef MonoThread* (*mono_thread_attach_fn)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_fn)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_fn)(MonoAssembly*);
typedef MonoClass* (*mono_class_from_name_fn)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_method_from_name_fn)(MonoClass*, const char*, int);
typedef MonoString* (*mono_string_new_fn)(MonoDomain*, const char*);
typedef MonoDomain* (*mono_domain_get_fn)(void);
typedef MonoObject* (*mono_runtime_invoke_fn)(MonoMethod*, void*, void**, MonoObject**);
typedef MonoProperty* (*mono_class_get_property_from_name_fn)(MonoClass*, const char*);
typedef MonoMethod* (*mono_property_get_get_method_fn)(MonoProperty*);
typedef MonoMethod* (*mono_property_get_set_method_fn)(MonoProperty*);
typedef void* (*mono_compile_method_fn)(MonoMethod*);
typedef MonoObject* (*mono_object_new_fn)(MonoDomain*, MonoClass*);
typedef void* (*mono_object_unbox_fn)(MonoObject*);
typedef void (*mono_runtime_object_init_fn)(MonoObject*);

struct phuf_packet {
    uint32_t magic;
    uint32_t version;
    uint64_t sequence;
    double fps;
    uint64_t reserved;
    char text[PHUF_TEXT_SIZE];
};

struct health_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    double fps;
    uint32_t loading;
    uint32_t reserved;
};

_Static_assert(sizeof(struct phuf_packet) == 0x420, "PHUF wire size");
_Static_assert(sizeof(struct health_packet) == 0x20, "health wire size");

static int health_socket = -1;
static struct sockaddr_in health_dst;
static uint64_t context_mask = 0;

static mono_get_root_domain_fn g_mono_get_root_domain;
static mono_thread_attach_fn g_mono_thread_attach;
static mono_domain_assembly_open_fn g_mono_domain_assembly_open;
static mono_assembly_get_image_fn g_mono_assembly_get_image;
static mono_class_from_name_fn g_mono_class_from_name;
static mono_class_get_method_from_name_fn g_mono_class_get_method_from_name;
static mono_string_new_fn g_mono_string_new;
static mono_domain_get_fn g_mono_domain_get;
static mono_runtime_invoke_fn g_mono_runtime_invoke;
static mono_class_get_property_from_name_fn g_mono_class_get_property_from_name;
static mono_property_get_get_method_fn g_mono_property_get_get_method;
static mono_property_get_set_method_fn g_mono_property_get_set_method;
static mono_compile_method_fn g_mono_compile_method;
static mono_object_new_fn g_mono_object_new;
static mono_object_unbox_fn g_mono_object_unbox;
static mono_runtime_object_init_fn g_mono_runtime_object_init;

static MonoDomain* g_root_domain;
static MonoImage* g_pui_image;
static MonoObject* g_root_widget;

static void send_health(uint32_t kind, uint64_t sequence,
                        double fps, uint32_t detail) {
    struct health_packet h;
    memset(&h, 0, sizeof(h));
    h.magic = HEALTH_MAGIC;
    h.kind = kind;
    h.sequence = sequence;
    h.fps = fps;
    h.loading = detail;
    (void)sendto(health_socket, &h, sizeof(h), 0,
                 (const struct sockaddr*)&health_dst, sizeof(health_dst));
}

static void context_step(uint32_t stage) {
    send_health(HEALTH_CONTEXT, context_mask, 0.0, stage);
}

static void visual_step(uint32_t stage) {
    send_health(HEALTH_VISUAL, context_mask, 0.0, stage);
}

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

static void* compile_method(MonoMethod* method) {
    return method ? g_mono_compile_method(method) : 0;
}

static MonoMethod* property_setter(MonoClass* klass, const char* name) {
    MonoProperty* prop = g_mono_class_get_property_from_name(klass, name);
    return prop ? g_mono_property_get_set_method(prop) : 0;
}

static int set_string_property(MonoClass* klass, MonoObject* object,
                               const char* name, MonoString* value) {
    void* thunk = compile_method(property_setter(klass, name));
    if (!thunk) return -1;
    ((void (*)(MonoObject*, MonoString*))thunk)(object, value);
    return 0;
}

static int set_float_property(MonoClass* klass, MonoObject* object,
                              const char* name, float value) {
    void* thunk = compile_method(property_setter(klass, name));
    if (!thunk) return -1;
    ((void (*)(MonoObject*, float))thunk)(object, value);
    return 0;
}

static int set_int_property(MonoClass* klass, MonoObject* object,
                            const char* name, int value) {
    void* thunk = compile_method(property_setter(klass, name));
    if (!thunk) return -1;
    ((void (*)(MonoObject*, int))thunk)(object, value);
    return 0;
}

static int set_bool_property(MonoClass* klass, MonoObject* object,
                             const char* name, bool value) {
    void* thunk = compile_method(property_setter(klass, name));
    if (!thunk) return -1;
    ((void (*)(MonoObject*, bool))thunk)(object, value);
    return 0;
}

static int set_object_property_invoke(MonoClass* klass, MonoObject* object,
                                      const char* name, MonoObject* value) {
    MonoMethod* setter = property_setter(klass, name);
    if (!setter) return -1;
    void* args[1];
    args[0] = &value;
    MonoObject* exception = 0;
    (void)g_mono_runtime_invoke(setter, object, args, &exception);
    return exception ? -1 : 0;
}

static MonoObject* create_font(int size, int style, int weight) {
    MonoClass* klass = g_mono_class_from_name(
        g_pui_image, "Sce.PlayStation.PUI.UI2", "UIFont");
    if (!klass) return 0;
    MonoObject* boxed = g_mono_object_new(g_root_domain, klass);
    if (!boxed) return 0;
    void* real = g_mono_object_unbox(boxed);
    if (!real) return 0;
    MonoMethod* ctor = g_mono_class_get_method_from_name(klass, ".ctor", 3);
    void* thunk = compile_method(ctor);
    if (!thunk) return 0;
    ((void (*)(void*, int, int, int))thunk)(real, size, style, weight);
    return (MonoObject*)real;
}

static MonoObject* create_color(float r, float g, float b, float a) {
    MonoClass* klass = g_mono_class_from_name(
        g_pui_image, "Sce.PlayStation.PUI", "UIColor");
    if (!klass) return 0;
    MonoObject* boxed = g_mono_object_new(g_root_domain, klass);
    if (!boxed) return 0;
    void* real = g_mono_object_unbox(boxed);
    if (!real) return 0;
    MonoMethod* ctor = g_mono_class_get_method_from_name(klass, ".ctor", 4);
    void* thunk = compile_method(ctor);
    if (!thunk) return 0;
    ((void (*)(void*, float, float, float, float))thunk)(real, r, g, b, a);
    return (MonoObject*)real;
}

static MonoObject* create_label(const char* name, float x, float y,
                                const char* text, MonoObject* font,
                                int horizontal_alignment,
                                float r, float g, float b, float a) {
    MonoClass* label_class = g_mono_class_from_name(
        g_pui_image, "Sce.PlayStation.PUI.UI2", "Label");
    if (!label_class) return 0;

    MonoObject* label = g_mono_object_new(g_root_domain, label_class);
    if (!label) return 0;
    g_mono_runtime_object_init(label);

    MonoString* mono_name = g_mono_string_new(g_root_domain, name);
    MonoString* mono_text = g_mono_string_new(g_root_domain, text);
    MonoObject* color = create_color(r, g, b, a);
    if (!mono_name || !mono_text || !color) return 0;

    if (set_string_property(label_class, label, "Name", mono_name) != 0 ||
        set_float_property(label_class, label, "X", x) != 0 ||
        set_float_property(label_class, label, "Y", y) != 0 ||
        set_string_property(label_class, label, "Text", mono_text) != 0 ||
        set_object_property_invoke(label_class, label, "Font", font) != 0 ||
        set_int_property(label_class, label, "HorizontalAlignment", horizontal_alignment) != 0 ||
        set_int_property(label_class, label, "VerticalAlignment", 0) != 0 ||
        set_object_property_invoke(label_class, label, "TextColor", color) != 0 ||
        set_bool_property(label_class, label, "FitWidthToText", true) != 0 ||
        set_bool_property(label_class, label, "FitHeightToText", true) != 0) {
        return 0;
    }

    return label;
}

static int append_child(MonoObject* child) {
    MonoClass* widget_class = g_mono_class_from_name(
        g_pui_image, "Sce.PlayStation.PUI.UI2", "Widget");
    if (!widget_class || !child || !g_root_widget) return -1;
    MonoMethod* append = g_mono_class_get_method_from_name(widget_class, "AppendChild", 1);
    if (!append) return -1;
    void* args[1];
    args[0] = child; /* exact etaHEN Widget_Append_Child calling convention */
    MonoObject* exception = 0;
    (void)g_mono_runtime_invoke(append, g_root_widget, args, &exception);
    return exception ? -1 : 0;
}

static int create_static_visual(void) {
    MonoObject* font = create_font(26, 0, 0);
    if (!font) {
        visual_step(1);
        return 41;
    }
    visual_step(1);

    MonoObject* label = create_label(
        "id_commonfps_sr9b_label", 10.0f, 1000.0f, "FPS:", font,
        1, 0.702f, 0.400f, 1.000f, 1.000f);
    if (!label) {
        visual_step(2);
        return 42;
    }
    visual_step(2);

    if (append_child(label) != 0) {
        visual_step(3);
        return 43;
    }
    visual_step(3);

    MonoObject* value = create_label(
        "id_commonfps_sr9b_value", 85.0f, 1000.0f, "loading", font,
        0, 1.000f, 1.000f, 1.000f, 1.000f);
    if (!value) {
        visual_step(4);
        return 44;
    }
    if (append_child(value) != 0) {
        visual_step(4);
        return 45;
    }
    visual_step(4);

    visual_step(VISUAL_READY_STAGE);
    return 0;
}

static int init_pui_context(void) {
    uint32_t mono_handle = 0;
    if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.sprx", &mono_handle) != 0 ||
        mono_handle == 0) {
        mono_handle = 0;
        if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.0.sprx", &mono_handle) != 0 ||
            mono_handle == 0) {
            context_step(1);
            return 21;
        }
    }
    context_mask |= 1ull << 0;
    context_step(1);

    RESOLVE_MONO(mono_handle, mono_get_root_domain, mono_get_root_domain_fn);
    RESOLVE_MONO(mono_handle, mono_thread_attach, mono_thread_attach_fn);
    RESOLVE_MONO(mono_handle, mono_domain_assembly_open, mono_domain_assembly_open_fn);
    RESOLVE_MONO(mono_handle, mono_assembly_get_image, mono_assembly_get_image_fn);
    RESOLVE_MONO(mono_handle, mono_class_from_name, mono_class_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_method_from_name, mono_class_get_method_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_string_new, mono_string_new_fn);
    RESOLVE_MONO(mono_handle, mono_domain_get, mono_domain_get_fn);
    RESOLVE_MONO(mono_handle, mono_runtime_invoke, mono_runtime_invoke_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_property_from_name, mono_class_get_property_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_property_get_get_method, mono_property_get_get_method_fn);
    RESOLVE_MONO(mono_handle, mono_property_get_set_method, mono_property_get_set_method_fn);
    RESOLVE_MONO(mono_handle, mono_compile_method, mono_compile_method_fn);
    RESOLVE_MONO(mono_handle, mono_object_new, mono_object_new_fn);
    RESOLVE_MONO(mono_handle, mono_object_unbox, mono_object_unbox_fn);
    RESOLVE_MONO(mono_handle, mono_runtime_object_init, mono_runtime_object_init_fn);

    if (!g_mono_get_root_domain || !g_mono_thread_attach ||
        !g_mono_domain_assembly_open || !g_mono_assembly_get_image ||
        !g_mono_class_from_name || !g_mono_class_get_method_from_name ||
        !g_mono_string_new || !g_mono_domain_get || !g_mono_runtime_invoke ||
        !g_mono_class_get_property_from_name || !g_mono_property_get_get_method ||
        !g_mono_property_get_set_method || !g_mono_compile_method ||
        !g_mono_object_new || !g_mono_object_unbox || !g_mono_runtime_object_init) {
        context_step(2);
        return 22;
    }
    context_mask |= 1ull << 1;
    context_step(2);

    g_root_domain = g_mono_get_root_domain();
    if (!g_root_domain) {
        context_step(3);
        return 23;
    }
    context_mask |= 1ull << 2;
    context_step(3);

    MonoThread* attached = g_mono_thread_attach(g_root_domain);
    if (!attached) {
        context_step(4);
        return 24;
    }
    context_mask |= 1ull << 3;
    context_step(4);

    MonoAssembly* pui_assembly = g_mono_domain_assembly_open(
        g_root_domain, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    g_pui_image = pui_assembly ? g_mono_assembly_get_image(pui_assembly) : 0;
    if (!g_pui_image) {
        context_step(5);
        return 25;
    }
    context_mask |= 1ull << 4;
    context_step(5);

    MonoAssembly* app_assembly = g_mono_domain_assembly_open(
        g_root_domain, "/system_ex/common_ex/lib/Sce.Vsh.ShellUI.AppSystem.dll");
    MonoImage* app_image = app_assembly ? g_mono_assembly_get_image(app_assembly) : 0;
    if (!app_image) {
        context_step(6);
        return 26;
    }
    context_mask |= 1ull << 5;
    context_step(6);

    MonoClass* layer = g_mono_class_from_name(
        app_image, "Sce.Vsh.ShellUI.AppSystem", "LayerManager");
    if (!layer) {
        context_step(7);
        return 27;
    }
    context_mask |= 1ull << 6;
    context_step(7);

    MonoMethod* find_scene = g_mono_class_get_method_from_name(
        layer, "FindContainerSceneByPath", 1);
    if (!find_scene) {
        context_step(8);
        return 28;
    }
    context_mask |= 1ull << 7;
    context_step(8);

    MonoDomain* current_domain = g_mono_domain_get();
    if (!current_domain) current_domain = g_root_domain;
    MonoString* game_path = g_mono_string_new(current_domain, "Game");
    if (!game_path) {
        context_step(9);
        return 29;
    }
    context_mask |= 1ull << 8;
    context_step(9);

    void* args[1];
    args[0] = game_path;
    MonoObject* exception = 0;
    MonoObject* game = g_mono_runtime_invoke(find_scene, 0, args, &exception);
    if (!game || exception) {
        context_step(10);
        return 30;
    }
    context_mask |= 1ull << 9;
    context_step(10);

    MonoClass* scene = g_mono_class_from_name(
        g_pui_image, "Sce.PlayStation.PUI.UI2", "Scene");
    if (!scene) {
        context_step(11);
        return 31;
    }
    context_mask |= 1ull << 10;
    context_step(11);

    MonoProperty* root_prop = g_mono_class_get_property_from_name(scene, "RootWidget");
    if (!root_prop) {
        context_step(12);
        return 32;
    }
    context_mask |= 1ull << 11;
    context_step(12);

    MonoMethod* root_getter = g_mono_property_get_get_method(root_prop);
    if (!root_getter) {
        context_step(13);
        return 33;
    }
    context_mask |= 1ull << 12;
    context_step(13);

    exception = 0;
    g_root_widget = g_mono_runtime_invoke(root_getter, game, 0, &exception);
    if (!g_root_widget || exception) {
        context_step(14);
        return 34;
    }
    context_mask |= 1ull << 13;
    context_step(14);

    context_mask |= 1ull << 14;
    context_step(15);
    return context_mask == CONTEXT_FULL_MASK ? 0 : 35;
}

int main(void) {
    health_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (health_socket < 0) return 10;
    memset(&health_dst, 0, sizeof(health_dst));
    health_dst.sin_family = AF_INET;
    health_dst.sin_port = HEALTH_PORT_NETWORK;
    health_dst.sin_addr.s_addr = LOOPBACK_ADDRESS;

    const int context_rc = init_pui_context();
    if (context_rc != 0) return context_rc;

    const int visual_rc = create_static_visual();
    if (visual_rc != 0) return visual_rc;

    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0) return 11;
    int reuse = 1;
    (void)setsockopt(receiver, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = PHUF_PORT_NETWORK;
    local.sin_addr.s_addr = LOOPBACK_ADDRESS;
    if (bind(receiver, (const struct sockaddr*)&local, sizeof(local)) != 0) {
        close(receiver);
        return 12;
    }

    send_health(HEALTH_READY, context_mask, 0.0, VISUAL_READY_STAGE);

    uint64_t last_sequence = 0;
    for (;;) {
        struct phuf_packet packet;
        const ssize_t got = recv(receiver, &packet, sizeof(packet), 0);
        if (got != (ssize_t)sizeof(packet)) {
            usleep(10000);
            continue;
        }
        if (packet.magic != PHUF_MAGIC || packet.version != PHUF_VERSION)
            continue;
        if (packet.sequence == 0 || packet.sequence <= last_sequence)
            continue;

        last_sequence = packet.sequence;
        const uint32_t loading =
            (packet.fps == 0.0 && packet.text[0] != '\0') ? 1u : 0u;
        send_health(HEALTH_PACKET, packet.sequence, packet.fps, loading);
    }
}
