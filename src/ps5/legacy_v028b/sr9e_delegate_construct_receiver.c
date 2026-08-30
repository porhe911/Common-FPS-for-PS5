/*
 * Common FPS v0.28b SR9E - System.Action construction-only receiver
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Runs in SceShellUI after the hardware-proven SR9D bootstrap. This stage
 * performs NO EnqueueEventAction call and NO UI mutation. It only constructs
 * a System.Action delegate bound to Application.RequestSwapBuffersIfNoDraw,
 * verifies that the resulting object is System.Action, reports the result over
 * loopback UDP, and exits.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS 0x0100007fu
#define RESULT_PORT_NETWORK 0xF9D8u /* htons(55545) */
#define RESULT_MAGIC 0x45394544u      /* "DE9E" */

#define KIND_STAGE 1u
#define KIND_RESULT 2u
#define KIND_DONE 3u
#define KIND_ERROR 4u

typedef void MonoDomain;
typedef void MonoThread;
typedef void MonoAssembly;
typedef void MonoImage;
typedef void MonoClass;
typedef void MonoMethod;
typedef void MonoObject;

typedef MonoDomain* (*mono_get_root_domain_fn)(void);
typedef MonoThread* (*mono_thread_attach_fn)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_fn)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_fn)(MonoAssembly*);
typedef MonoImage* (*mono_get_corlib_fn)(void);
typedef MonoClass* (*mono_class_from_name_fn)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_method_from_name_fn)(MonoClass*, const char*, int);
typedef MonoObject* (*mono_object_new_fn)(MonoDomain*, MonoClass*);
typedef void* (*mono_compile_method_fn)(MonoMethod*);
typedef MonoObject* (*mono_runtime_invoke_fn)(MonoMethod*, void*, void**, MonoObject**);
typedef MonoClass* (*mono_object_get_class_fn)(MonoObject*);
typedef const char* (*mono_class_get_name_fn)(MonoClass*);

struct result_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    uint32_t code;
    uint32_t reserved;
    char detail[96];
    char extra[160];
};

static int g_socket = -1;
static struct sockaddr_in g_destination;
static uint64_t g_sequence = 1;

static mono_get_root_domain_fn g_mono_get_root_domain;
static mono_thread_attach_fn g_mono_thread_attach;
static mono_domain_assembly_open_fn g_mono_domain_assembly_open;
static mono_assembly_get_image_fn g_mono_assembly_get_image;
static mono_get_corlib_fn g_mono_get_corlib;
static mono_class_from_name_fn g_mono_class_from_name;
static mono_class_get_method_from_name_fn g_mono_class_get_method_from_name;
static mono_object_new_fn g_mono_object_new;
static mono_compile_method_fn g_mono_compile_method;
static mono_runtime_invoke_fn g_mono_runtime_invoke;
static mono_object_get_class_fn g_mono_object_get_class;
static mono_class_get_name_fn g_mono_class_get_name;

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

static void send_packet(uint32_t kind, uint32_t code,
                        const char* detail, const char* extra) {
    struct result_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.magic = RESULT_MAGIC;
    packet.kind = kind;
    packet.sequence = g_sequence++;
    packet.code = code;
    if (detail) strncpy(packet.detail, detail, sizeof(packet.detail) - 1);
    if (extra) strncpy(packet.extra, extra, sizeof(packet.extra) - 1);
    (void)sendto(g_socket, &packet, sizeof(packet), 0,
                 (const struct sockaddr*)&g_destination, sizeof(g_destination));
}

static int fail(uint32_t code, const char* detail, const char* extra) {
    send_packet(KIND_ERROR, code, detail, extra);
    if (g_socket >= 0) close(g_socket);
    return (int)code;
}

int main(void) {
    g_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket < 0) return 10;
    memset(&g_destination, 0, sizeof(g_destination));
    g_destination.sin_family = AF_INET;
    g_destination.sin_port = RESULT_PORT_NETWORK;
    g_destination.sin_addr.s_addr = LOOPBACK_ADDRESS;
    send_packet(KIND_STAGE, 0, "SR9E", "START_DELEGATE_CONSTRUCT_ONLY");

    uint32_t mono_handle = 0;
    if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.sprx", &mono_handle) != 0 || mono_handle == 0) {
        mono_handle = 0;
        if (kernel_dynlib_handle(getpid(), "libmonosgen-2.0.0.sprx", &mono_handle) != 0 || mono_handle == 0)
            return fail(21, "Mono", "MODULE_NOT_FOUND");
    }

    RESOLVE_MONO(mono_handle, mono_get_root_domain, mono_get_root_domain_fn);
    RESOLVE_MONO(mono_handle, mono_thread_attach, mono_thread_attach_fn);
    RESOLVE_MONO(mono_handle, mono_domain_assembly_open, mono_domain_assembly_open_fn);
    RESOLVE_MONO(mono_handle, mono_assembly_get_image, mono_assembly_get_image_fn);
    RESOLVE_MONO(mono_handle, mono_get_corlib, mono_get_corlib_fn);
    RESOLVE_MONO(mono_handle, mono_class_from_name, mono_class_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_method_from_name, mono_class_get_method_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_object_new, mono_object_new_fn);
    RESOLVE_MONO(mono_handle, mono_compile_method, mono_compile_method_fn);
    RESOLVE_MONO(mono_handle, mono_runtime_invoke, mono_runtime_invoke_fn);
    RESOLVE_MONO(mono_handle, mono_object_get_class, mono_object_get_class_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_name, mono_class_get_name_fn);

    if (!g_mono_get_root_domain || !g_mono_thread_attach || !g_mono_domain_assembly_open ||
        !g_mono_assembly_get_image || !g_mono_get_corlib || !g_mono_class_from_name ||
        !g_mono_class_get_method_from_name || !g_mono_object_new || !g_mono_compile_method ||
        !g_mono_runtime_invoke || !g_mono_object_get_class || !g_mono_class_get_name)
        return fail(22, "Mono", "REQUIRED_SYMBOL_MISSING");

    MonoDomain* root = g_mono_get_root_domain();
    if (!root || !g_mono_thread_attach(root))
        return fail(23, "Mono", "THREAD_ATTACH_FAILED");
    send_packet(KIND_STAGE, 0, "Mono", "THREAD_ATTACHED");

    MonoAssembly* assembly = g_mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    MonoImage* pui = assembly ? g_mono_assembly_get_image(assembly) : 0;
    if (!pui) return fail(24, "PUI", "ASSEMBLY_OPEN_FAILED");

    MonoClass* app_class = g_mono_class_from_name(pui, "Sce.PlayStation.PUI", "Application");
    if (!app_class) return fail(25, "Application", "CLASS_NOT_FOUND");

    MonoMethod* get_instance = g_mono_class_get_method_from_name(app_class, "get_Instance", 0);
    MonoMethod* target = g_mono_class_get_method_from_name(app_class, "RequestSwapBuffersIfNoDraw", 0);
    if (!get_instance || !target)
        return fail(26, "Application", "TARGET_METHOD_NOT_FOUND");

    MonoObject* exc = 0;
    MonoObject* app_instance = g_mono_runtime_invoke(get_instance, 0, 0, &exc);
    if (exc || !app_instance)
        return fail(27, "Application", "GET_INSTANCE_FAILED");
    send_packet(KIND_STAGE, 0, "Application", "INSTANCE_READY");

    MonoImage* corlib = g_mono_get_corlib();
    if (!corlib) return fail(28, "System", "CORLIB_NOT_FOUND");
    MonoClass* action_class = g_mono_class_from_name(corlib, "System", "Action");
    if (!action_class) return fail(29, "System.Action", "CLASS_NOT_FOUND");
    MonoMethod* action_ctor = g_mono_class_get_method_from_name(action_class, ".ctor", 2);
    if (!action_ctor) return fail(30, "System.Action", "CTOR_NOT_FOUND");

    MonoObject* action = g_mono_object_new(root, action_class);
    if (!action) return fail(31, "System.Action", "OBJECT_NEW_FAILED");

    void* compiled_target = g_mono_compile_method(target);
    if (!compiled_target) return fail(32, "Application", "TARGET_COMPILE_FAILED");

    void* ctor_args[2];
    ctor_args[0] = app_instance;
    ctor_args[1] = &compiled_target;
    exc = 0;
    (void)g_mono_runtime_invoke(action_ctor, action, ctor_args, &exc);
    if (exc) return fail(33, "System.Action", "CTOR_EXCEPTION");

    MonoClass* actual_class = g_mono_object_get_class(action);
    const char* actual_name = actual_class ? g_mono_class_get_name(actual_class) : 0;
    if (!actual_class || !actual_name || strcmp(actual_name, "Action") != 0)
        return fail(34, "System.Action", "TYPE_VERIFY_FAILED");

    send_packet(KIND_RESULT, 0, "DELEGATE_CONSTRUCTED", actual_name);
    send_packet(KIND_DONE, 0, "SR9E", "DONE_NO_INVOKE_NO_ENQUEUE");
    close(g_socket);
    return 0;
}
