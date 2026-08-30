/*
 * Common FPS v0.28b SR9A - PUI context-only ShellUI receiver
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Builds directly on the hardware-proven SR8B receiver/bootstrap. This stage
 * resolves and validates the exact etaHEN Mono/PUI context chain but never
 * creates, removes, or updates a widget. Progress is reported over UDP 55542.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
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
#define HEALTH_MAGIC 0x41394852u /* "RH9A" in memory */
#define HEALTH_READY 1u
#define HEALTH_PACKET 2u
#define HEALTH_CONTEXT 3u
#define CONTEXT_FULL_MASK 0x7fffu

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

#define RESOLVE_MONO(handle, name, type) \
    type name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name)

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

    if (!mono_get_root_domain || !mono_thread_attach ||
        !mono_domain_assembly_open || !mono_assembly_get_image ||
        !mono_class_from_name || !mono_class_get_method_from_name ||
        !mono_string_new || !mono_domain_get || !mono_runtime_invoke ||
        !mono_class_get_property_from_name || !mono_property_get_get_method) {
        context_step(2);
        return 22;
    }
    context_mask |= 1ull << 1;
    context_step(2);

    MonoDomain* root = mono_get_root_domain();
    if (!root) {
        context_step(3);
        return 23;
    }
    context_mask |= 1ull << 2;
    context_step(3);

    MonoThread* attached = mono_thread_attach(root);
    if (!attached) {
        context_step(4);
        return 24;
    }
    context_mask |= 1ull << 3;
    context_step(4);

    MonoAssembly* pui_assembly = mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    MonoImage* pui_image = pui_assembly ? mono_assembly_get_image(pui_assembly) : 0;
    if (!pui_image) {
        context_step(5);
        return 25;
    }
    context_mask |= 1ull << 4;
    context_step(5);

    MonoAssembly* app_assembly = mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.Vsh.ShellUI.AppSystem.dll");
    MonoImage* app_image = app_assembly ? mono_assembly_get_image(app_assembly) : 0;
    if (!app_image) {
        context_step(6);
        return 26;
    }
    context_mask |= 1ull << 5;
    context_step(6);

    MonoClass* layer = mono_class_from_name(
        app_image, "Sce.Vsh.ShellUI.AppSystem", "LayerManager");
    if (!layer) {
        context_step(7);
        return 27;
    }
    context_mask |= 1ull << 6;
    context_step(7);

    MonoMethod* find_scene = mono_class_get_method_from_name(
        layer, "FindContainerSceneByPath", 1);
    if (!find_scene) {
        context_step(8);
        return 28;
    }
    context_mask |= 1ull << 7;
    context_step(8);

    MonoDomain* current_domain = mono_domain_get();
    if (!current_domain)
        current_domain = root;
    MonoString* game_path = mono_string_new(current_domain, "Game");
    if (!game_path) {
        context_step(9);
        return 29;
    }
    context_mask |= 1ull << 8;
    context_step(9);

    void* args[1];
    args[0] = game_path;
    MonoObject* exception = 0;
    MonoObject* game = mono_runtime_invoke(find_scene, 0, args, &exception);
    if (!game || exception) {
        context_step(10);
        return 30;
    }
    context_mask |= 1ull << 9;
    context_step(10);

    MonoClass* scene = mono_class_from_name(
        pui_image, "Sce.PlayStation.PUI.UI2", "Scene");
    if (!scene) {
        context_step(11);
        return 31;
    }
    context_mask |= 1ull << 10;
    context_step(11);

    MonoProperty* root_prop = mono_class_get_property_from_name(scene, "RootWidget");
    if (!root_prop) {
        context_step(12);
        return 32;
    }
    context_mask |= 1ull << 11;
    context_step(12);

    MonoMethod* root_getter = mono_property_get_get_method(root_prop);
    if (!root_getter) {
        context_step(13);
        return 33;
    }
    context_mask |= 1ull << 12;
    context_step(13);

    exception = 0;
    MonoObject* root_widget = mono_runtime_invoke(root_getter, game, 0, &exception);
    if (!root_widget || exception) {
        context_step(14);
        return 34;
    }
    context_mask |= 1ull << 13;
    context_step(14);

    /* Final bit means the exact etaHEN context chain is fully usable. */
    context_mask |= 1ull << 14;
    context_step(15);
    return context_mask == CONTEXT_FULL_MASK ? 0 : 35;
}

int main(void) {
    health_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (health_socket < 0)
        return 10;
    memset(&health_dst, 0, sizeof(health_dst));
    health_dst.sin_family = AF_INET;
    health_dst.sin_port = HEALTH_PORT_NETWORK;
    health_dst.sin_addr.s_addr = LOOPBACK_ADDRESS;

    const int context_rc = init_pui_context();
    if (context_rc != 0)
        return context_rc;

    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver < 0)
        return 11;
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

    send_health(HEALTH_READY, context_mask, 0.0, 15);

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
