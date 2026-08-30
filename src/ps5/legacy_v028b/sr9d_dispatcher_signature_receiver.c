/*
 * Common FPS v0.28b SR9D - targeted PUI dispatcher signature receiver
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Metadata only. NO UI mutation, NO detour, NO code patch.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS 0x0100007fu
#define SIGNATURE_PORT_NETWORK 0xF8D8u /* htons(55544) */
#define SIGNATURE_MAGIC 0x44394453u      /* "SD9D" */

#define KIND_STAGE 1u
#define KIND_SIGNATURE 2u
#define KIND_DONE 3u
#define KIND_ERROR 4u
#define METHOD_ATTRIBUTE_STATIC 0x0010u

typedef void MonoDomain;
typedef void MonoThread;
typedef void MonoAssembly;
typedef void MonoImage;
typedef void MonoClass;
typedef void MonoMethod;
typedef void MonoMethodSignature;
typedef void MonoType;

typedef MonoDomain* (*mono_get_root_domain_fn)(void);
typedef MonoThread* (*mono_thread_attach_fn)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_fn)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_fn)(MonoAssembly*);
typedef MonoClass* (*mono_class_from_name_fn)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_methods_fn)(MonoClass*, void**);
typedef const char* (*mono_method_get_name_fn)(MonoMethod*);
typedef MonoMethodSignature* (*mono_method_signature_fn)(MonoMethod*);
typedef uint32_t (*mono_signature_get_param_count_fn)(MonoMethodSignature*);
typedef MonoType* (*mono_signature_get_return_type_fn)(MonoMethodSignature*);
typedef MonoType* (*mono_signature_get_params_fn)(MonoMethodSignature*, void**);
typedef char* (*mono_type_get_name_fn)(MonoType*);
typedef uint32_t (*mono_method_get_flags_fn)(MonoMethod*, uint32_t*);

struct signature_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    uint32_t flags;
    uint32_t param_count;
    char method_name[64];
    char return_type[96];
    char param_types[256];
};

static int g_socket = -1;
static struct sockaddr_in g_destination;
static uint64_t g_sequence = 1;

static mono_get_root_domain_fn g_mono_get_root_domain;
static mono_thread_attach_fn g_mono_thread_attach;
static mono_domain_assembly_open_fn g_mono_domain_assembly_open;
static mono_assembly_get_image_fn g_mono_assembly_get_image;
static mono_class_from_name_fn g_mono_class_from_name;
static mono_class_get_methods_fn g_mono_class_get_methods;
static mono_method_get_name_fn g_mono_method_get_name;
static mono_method_signature_fn g_mono_method_signature;
static mono_signature_get_param_count_fn g_mono_signature_get_param_count;
static mono_signature_get_return_type_fn g_mono_signature_get_return_type;
static mono_signature_get_params_fn g_mono_signature_get_params;
static mono_type_get_name_fn g_mono_type_get_name;
static mono_method_get_flags_fn g_mono_method_get_flags;

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

static void send_packet(uint32_t kind, uint32_t flags, uint32_t param_count,
                        const char* method_name, const char* return_type,
                        const char* param_types) {
    struct signature_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.magic = SIGNATURE_MAGIC;
    packet.kind = kind;
    packet.sequence = g_sequence++;
    packet.flags = flags;
    packet.param_count = param_count;
    if (method_name) strncpy(packet.method_name, method_name, sizeof(packet.method_name) - 1);
    if (return_type) strncpy(packet.return_type, return_type, sizeof(packet.return_type) - 1);
    if (param_types) strncpy(packet.param_types, param_types, sizeof(packet.param_types) - 1);
    (void)sendto(g_socket, &packet, sizeof(packet), 0,
                 (const struct sockaddr*)&g_destination, sizeof(g_destination));
}

static bool is_target(const char* name) {
    static const char* targets[] = {
        "EnqueueEventAction",
        "add_FrameBegun",
        "_SetSynchronizationContext",
        "get_Instance",
        "Update",
        "onRender",
        "Render"
    };
    if (!name) return false;
    for (unsigned i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i)
        if (strcmp(name, targets[i]) == 0) return true;
    return false;
}

static void append_text(char* dst, size_t cap, const char* text) {
    if (!dst || !cap || !text) return;
    const size_t used = strlen(dst);
    if (used + 1 >= cap) return;
    strncat(dst, text, cap - used - 1);
}

int main(void) {
    g_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket < 0) return 10;
    memset(&g_destination, 0, sizeof(g_destination));
    g_destination.sin_family = AF_INET;
    g_destination.sin_port = SIGNATURE_PORT_NETWORK;
    g_destination.sin_addr.s_addr = LOOPBACK_ADDRESS;
    send_packet(KIND_STAGE, 0, 0, "SR9D", "START_SIGNATURE_ONLY", "");

    uint32_t mono_handle = 0;
    if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.sprx", &mono_handle) != 0 || mono_handle == 0) {
        mono_handle = 0;
        if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.0.sprx", &mono_handle) != 0 || mono_handle == 0) {
            send_packet(KIND_ERROR, 0, 21, "Mono", "MODULE_NOT_FOUND", "");
            return 21;
        }
    }

    RESOLVE_MONO(mono_handle, mono_get_root_domain, mono_get_root_domain_fn);
    RESOLVE_MONO(mono_handle, mono_thread_attach, mono_thread_attach_fn);
    RESOLVE_MONO(mono_handle, mono_domain_assembly_open, mono_domain_assembly_open_fn);
    RESOLVE_MONO(mono_handle, mono_assembly_get_image, mono_assembly_get_image_fn);
    RESOLVE_MONO(mono_handle, mono_class_from_name, mono_class_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_methods, mono_class_get_methods_fn);
    RESOLVE_MONO(mono_handle, mono_method_get_name, mono_method_get_name_fn);
    RESOLVE_MONO(mono_handle, mono_method_signature, mono_method_signature_fn);
    RESOLVE_MONO(mono_handle, mono_signature_get_param_count, mono_signature_get_param_count_fn);
    RESOLVE_MONO(mono_handle, mono_signature_get_return_type, mono_signature_get_return_type_fn);
    RESOLVE_MONO(mono_handle, mono_signature_get_params, mono_signature_get_params_fn);
    RESOLVE_MONO(mono_handle, mono_type_get_name, mono_type_get_name_fn);
    RESOLVE_MONO(mono_handle, mono_method_get_flags, mono_method_get_flags_fn);

    if (!g_mono_get_root_domain || !g_mono_thread_attach || !g_mono_domain_assembly_open ||
        !g_mono_assembly_get_image || !g_mono_class_from_name || !g_mono_class_get_methods ||
        !g_mono_method_get_name || !g_mono_method_signature || !g_mono_signature_get_param_count ||
        !g_mono_signature_get_return_type || !g_mono_signature_get_params || !g_mono_type_get_name ||
        !g_mono_method_get_flags) {
        send_packet(KIND_ERROR, 0, 22, "Mono", "REQUIRED_SYMBOL_MISSING", "");
        return 22;
    }

    MonoDomain* root = g_mono_get_root_domain();
    if (!root || !g_mono_thread_attach(root)) {
        send_packet(KIND_ERROR, 0, 23, "Mono", "THREAD_ATTACH_FAILED", "");
        return 23;
    }

    MonoAssembly* assembly = g_mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    MonoImage* image = assembly ? g_mono_assembly_get_image(assembly) : 0;
    if (!image) {
        send_packet(KIND_ERROR, 0, 24, "PUI", "ASSEMBLY_OPEN_FAILED", "");
        return 24;
    }

    MonoClass* app = g_mono_class_from_name(image, "Sce.PlayStation.PUI", "Application");
    if (!app) {
        send_packet(KIND_ERROR, 0, 25, "Application", "CLASS_NOT_FOUND", "");
        return 25;
    }

    unsigned found = 0;
    void* iter = 0;
    for (;;) {
        MonoMethod* method = g_mono_class_get_methods(app, &iter);
        if (!method) break;
        const char* name = g_mono_method_get_name(method);
        if (!is_target(name)) continue;

        MonoMethodSignature* sig = g_mono_method_signature(method);
        if (!sig) continue;
        const uint32_t count = g_mono_signature_get_param_count(sig);
        uint32_t iflags = 0;
        const uint32_t flags = g_mono_method_get_flags(method, &iflags);

        char retbuf[96] = {0};
        MonoType* ret = g_mono_signature_get_return_type(sig);
        if (ret) {
            char* rn = g_mono_type_get_name(ret);
            if (rn) strncpy(retbuf, rn, sizeof(retbuf) - 1);
        }

        char params[256] = {0};
        void* piter = 0;
        for (uint32_t i = 0; i < count; ++i) {
            MonoType* pt = g_mono_signature_get_params(sig, &piter);
            if (!pt) break;
            char* pn = g_mono_type_get_name(pt);
            if (i) append_text(params, sizeof(params), ", ");
            append_text(params, sizeof(params), pn ? pn : "?");
        }

        send_packet(KIND_SIGNATURE, flags, count, name, retbuf, params);
        ++found;
        usleep(1000);
    }

    send_packet(KIND_DONE, 0, found, "SR9D", "DONE", "");
    close(g_socket);
    return 0;
}
