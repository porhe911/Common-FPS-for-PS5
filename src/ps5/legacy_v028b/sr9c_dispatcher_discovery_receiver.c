/*
 * Common FPS v0.28b SR9C - metadata-only PUI dispatcher discovery receiver
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Runs inside SceShellUI after the hardware-proven SR9A bootstrap.  This stage
 * performs NO UI mutation and installs NO detour.  It only attaches the worker
 * to Mono, opens Sce.PlayStation.PUI.dll, enumerates method metadata for a
 * small set of PUI classes, and reports candidate main/UI-thread dispatch
 * methods to the controller over loopback UDP.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS 0x0100007fu
#define DISCOVERY_PORT_NETWORK 0xF7D8u /* htons(55543) on little-endian x86-64 */
#define DISCOVERY_MAGIC 0x43394452u      /* "RD9C" in memory */

#define KIND_STAGE 1u
#define KIND_METHOD 2u
#define KIND_DONE 3u
#define KIND_ERROR 4u

typedef void MonoDomain;
typedef void MonoThread;
typedef void MonoAssembly;
typedef void MonoImage;
typedef void MonoClass;
typedef void MonoMethod;
typedef void MonoMethodSignature;

typedef MonoDomain* (*mono_get_root_domain_fn)(void);
typedef MonoThread* (*mono_thread_attach_fn)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_fn)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_fn)(MonoAssembly*);
typedef MonoClass* (*mono_class_from_name_fn)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_methods_fn)(MonoClass*, void**);
typedef const char* (*mono_method_get_name_fn)(MonoMethod*);
typedef MonoMethodSignature* (*mono_method_signature_fn)(MonoMethod*);
typedef uint32_t (*mono_signature_get_param_count_fn)(MonoMethodSignature*);

struct discovery_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    uint32_t class_id;
    uint32_t param_count;
    char class_name[64];
    char method_name[96];
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

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

static void send_packet(uint32_t kind, uint32_t class_id, uint32_t param_count,
                        const char* class_name, const char* method_name) {
    struct discovery_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.magic = DISCOVERY_MAGIC;
    packet.kind = kind;
    packet.sequence = g_sequence++;
    packet.class_id = class_id;
    packet.param_count = param_count;
    if (class_name)
        strncpy(packet.class_name, class_name, sizeof(packet.class_name) - 1);
    if (method_name)
        strncpy(packet.method_name, method_name, sizeof(packet.method_name) - 1);
    (void)sendto(g_socket, &packet, sizeof(packet), 0,
                 (const struct sockaddr*)&g_destination, sizeof(g_destination));
}

static bool contains_candidate_word(const char* name) {
    if (!name || !*name) return false;
    static const char* needles[] = {
        "Update", "update", "Render", "render", "Invoke", "invoke",
        "Dispatch", "dispatch", "Post", "post", "Send", "send",
        "Main", "main", "Thread", "thread", "Callback", "callback",
        "Action", "action", "Tick", "tick", "Run", "run", "Async", "async"
    };
    for (unsigned i = 0; i < sizeof(needles) / sizeof(needles[0]); ++i) {
        if (strstr(name, needles[i])) return true;
    }
    return false;
}

static unsigned enumerate_class(MonoImage* image,
                                uint32_t class_id,
                                const char* name_space,
                                const char* class_name,
                                bool report_all) {
    MonoClass* klass = g_mono_class_from_name(image, name_space, class_name);
    if (!klass) {
        send_packet(KIND_STAGE, class_id, 0, class_name, "CLASS_NOT_FOUND");
        return 0;
    }

    send_packet(KIND_STAGE, class_id, 0, class_name, "CLASS_FOUND");

    unsigned reported = 0;
    void* iter = 0;
    for (;;) {
        MonoMethod* method = g_mono_class_get_methods(klass, &iter);
        if (!method) break;
        const char* method_name = g_mono_method_get_name(method);
        if (!method_name || !*method_name) continue;
        if (!report_all && !contains_candidate_word(method_name)) continue;

        uint32_t params = 0xffffffffu;
        MonoMethodSignature* signature = g_mono_method_signature(method);
        if (signature)
            params = g_mono_signature_get_param_count(signature);

        send_packet(KIND_METHOD, class_id, params, class_name, method_name);
        ++reported;
        usleep(1000);
    }
    return reported;
}

int main(void) {
    g_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket < 0) return 10;
    memset(&g_destination, 0, sizeof(g_destination));
    g_destination.sin_family = AF_INET;
    g_destination.sin_port = DISCOVERY_PORT_NETWORK;
    g_destination.sin_addr.s_addr = LOOPBACK_ADDRESS;

    send_packet(KIND_STAGE, 0, 0, "SR9C", "START_METADATA_ONLY");

    uint32_t mono_handle = 0;
    if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.sprx", &mono_handle) != 0 || mono_handle == 0) {
        mono_handle = 0;
        if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.0.sprx", &mono_handle) != 0 || mono_handle == 0) {
            send_packet(KIND_ERROR, 0, 21, "Mono", "MODULE_NOT_FOUND");
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

    if (!g_mono_get_root_domain || !g_mono_thread_attach ||
        !g_mono_domain_assembly_open || !g_mono_assembly_get_image ||
        !g_mono_class_from_name || !g_mono_class_get_methods ||
        !g_mono_method_get_name || !g_mono_method_signature ||
        !g_mono_signature_get_param_count) {
        send_packet(KIND_ERROR, 0, 22, "Mono", "REQUIRED_SYMBOL_MISSING");
        return 22;
    }

    MonoDomain* root = g_mono_get_root_domain();
    if (!root || !g_mono_thread_attach(root)) {
        send_packet(KIND_ERROR, 0, 23, "Mono", "THREAD_ATTACH_FAILED");
        return 23;
    }
    send_packet(KIND_STAGE, 0, 0, "Mono", "THREAD_ATTACHED");

    MonoAssembly* assembly = g_mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    MonoImage* image = assembly ? g_mono_assembly_get_image(assembly) : 0;
    if (!image) {
        send_packet(KIND_ERROR, 0, 24, "PUI", "ASSEMBLY_OPEN_FAILED");
        return 24;
    }
    send_packet(KIND_STAGE, 0, 0, "PUI", "ASSEMBLY_READY");

    unsigned total = 0;
    /* Application is small enough to report fully; adjacent UI classes are filtered. */
    total += enumerate_class(image, 1, "Sce.PlayStation.PUI", "Application", true);
    total += enumerate_class(image, 2, "Sce.PlayStation.PUI.UI2", "Scene", false);
    total += enumerate_class(image, 3, "Sce.PlayStation.PUI.UI2", "Widget", false);
    total += enumerate_class(image, 4, "Sce.PlayStation.PUI", "Dispatcher", true);
    total += enumerate_class(image, 5, "Sce.PlayStation.PUI", "UIThread", true);
    total += enumerate_class(image, 6, "Sce.PlayStation.PUI.UI2", "Application", true);

    send_packet(KIND_DONE, 0, total, "SR9C", "DONE");
    close(g_socket);
    return 0;
}
