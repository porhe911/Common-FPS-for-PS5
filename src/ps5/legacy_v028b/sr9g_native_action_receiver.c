/*
 * Common FPS v0.28b SR9G - native callback through PUI queue proof
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * NO UI mutation. This stage proves that Application.EnqueueEventAction can
 * execute a System.Action backed by a native function pointer from the injected
 * receiver. The native callback only flips an atomic flag. No widget is created,
 * removed, or modified.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS 0x0100007fu
#define RESULT_PORT_NETWORK 0xFBD8u /* htons(55547) */
#define RESULT_MAGIC 0x4739414eu      /* "NA9G" */

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
typedef void MonoString;
typedef void MonoObject;
typedef void MonoProperty;
typedef void MonoType;

typedef MonoDomain* (*mono_get_root_domain_fn)(void);
typedef MonoThread* (*mono_thread_attach_fn)(MonoDomain*);
typedef MonoAssembly* (*mono_domain_assembly_open_fn)(MonoDomain*, const char*);
typedef MonoImage* (*mono_assembly_get_image_fn)(MonoAssembly*);
typedef MonoImage* (*mono_get_corlib_fn)(void);
typedef MonoClass* (*mono_class_from_name_fn)(MonoImage*, const char*, const char*);
typedef MonoMethod* (*mono_class_get_method_from_name_fn)(MonoClass*, const char*, int);
typedef MonoString* (*mono_string_new_fn)(MonoDomain*, const char*);
typedef MonoDomain* (*mono_domain_get_fn)(void);
typedef MonoObject* (*mono_runtime_invoke_fn)(MonoMethod*, void*, void**, MonoObject**);
typedef MonoProperty* (*mono_class_get_property_from_name_fn)(MonoClass*, const char*);
typedef MonoMethod* (*mono_property_get_get_method_fn)(MonoProperty*);
typedef MonoType* (*mono_class_get_type_fn)(MonoClass*);
typedef MonoObject* (*mono_type_get_object_fn)(MonoDomain*, MonoType*);
typedef MonoClass* (*mono_object_get_class_fn)(MonoObject*);
typedef const char* (*mono_class_get_name_fn)(MonoClass*);

struct result_packet {
    uint32_t magic;
    uint32_t kind;
    uint64_t sequence;
    int32_t value;
    uint32_t code;
    char detail[96];
    char extra[160];
};

static int g_socket = -1;
static struct sockaddr_in g_destination;
static uint64_t g_sequence = 1;
static volatile uint32_t g_native_callback_seen = 0;

static mono_get_root_domain_fn g_mono_get_root_domain;
static mono_thread_attach_fn g_mono_thread_attach;
static mono_domain_assembly_open_fn g_mono_domain_assembly_open;
static mono_assembly_get_image_fn g_mono_assembly_get_image;
static mono_get_corlib_fn g_mono_get_corlib;
static mono_class_from_name_fn g_mono_class_from_name;
static mono_class_get_method_from_name_fn g_mono_class_get_method_from_name;
static mono_string_new_fn g_mono_string_new;
static mono_domain_get_fn g_mono_domain_get;
static mono_runtime_invoke_fn g_mono_runtime_invoke;
static mono_class_get_property_from_name_fn g_mono_class_get_property_from_name;
static mono_property_get_get_method_fn g_mono_property_get_get_method;
static mono_class_get_type_fn g_mono_class_get_type;
static mono_type_get_object_fn g_mono_type_get_object;
static mono_object_get_class_fn g_mono_object_get_class;
static mono_class_get_name_fn g_mono_class_get_name;

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

static void native_queue_callback(void) {
    __atomic_store_n((uint32_t*)&g_native_callback_seen, 1u, __ATOMIC_RELEASE);
}

static void send_packet(uint32_t kind, uint32_t code, int32_t value,
                        const char* detail, const char* extra) {
    struct result_packet packet;
    memset(&packet, 0, sizeof(packet));
    packet.magic = RESULT_MAGIC;
    packet.kind = kind;
    packet.sequence = g_sequence++;
    packet.value = value;
    packet.code = code;
    if (detail) strncpy(packet.detail, detail, sizeof(packet.detail) - 1);
    if (extra) strncpy(packet.extra, extra, sizeof(packet.extra) - 1);
    (void)sendto(g_socket, &packet, sizeof(packet), 0,
                 (const struct sockaddr*)&g_destination, sizeof(g_destination));
}

static int fail(uint32_t code, const char* detail, const char* extra) {
    send_packet(KIND_ERROR, code, -1, detail, extra);
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
    send_packet(KIND_STAGE, 0, 0, "SR9G", "START_NATIVE_ACTION_PROOF");

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
    RESOLVE_MONO(mono_handle, mono_string_new, mono_string_new_fn);
    RESOLVE_MONO(mono_handle, mono_domain_get, mono_domain_get_fn);
    RESOLVE_MONO(mono_handle, mono_runtime_invoke, mono_runtime_invoke_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_property_from_name, mono_class_get_property_from_name_fn);
    RESOLVE_MONO(mono_handle, mono_property_get_get_method, mono_property_get_get_method_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_type, mono_class_get_type_fn);
    RESOLVE_MONO(mono_handle, mono_type_get_object, mono_type_get_object_fn);
    RESOLVE_MONO(mono_handle, mono_object_get_class, mono_object_get_class_fn);
    RESOLVE_MONO(mono_handle, mono_class_get_name, mono_class_get_name_fn);

    if (!g_mono_get_root_domain || !g_mono_thread_attach || !g_mono_domain_assembly_open ||
        !g_mono_assembly_get_image || !g_mono_get_corlib || !g_mono_class_from_name ||
        !g_mono_class_get_method_from_name || !g_mono_string_new || !g_mono_domain_get ||
        !g_mono_runtime_invoke || !g_mono_class_get_property_from_name ||
        !g_mono_property_get_get_method || !g_mono_class_get_type || !g_mono_type_get_object ||
        !g_mono_object_get_class || !g_mono_class_get_name)
        return fail(22, "Mono", "REQUIRED_SYMBOL_MISSING");

    MonoDomain* root = g_mono_get_root_domain();
    if (!root || !g_mono_thread_attach(root))
        return fail(23, "Mono", "THREAD_ATTACH_FAILED");
    send_packet(KIND_STAGE, 0, 0, "Mono", "THREAD_ATTACHED");

    MonoAssembly* pui_assembly = g_mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.PlayStation.PUI.dll");
    MonoImage* pui = pui_assembly ? g_mono_assembly_get_image(pui_assembly) : 0;
    MonoAssembly* app_assembly = g_mono_domain_assembly_open(
        root, "/system_ex/common_ex/lib/Sce.Vsh.ShellUI.AppSystem.dll");
    MonoImage* app_image = app_assembly ? g_mono_assembly_get_image(app_assembly) : 0;
    MonoImage* corlib = g_mono_get_corlib();
    if (!pui || !app_image || !corlib)
        return fail(24, "Assemblies", "IMAGE_NOT_READY");

    MonoClass* application_class = g_mono_class_from_name(pui, "Sce.PlayStation.PUI", "Application");
    MonoMethod* get_instance = application_class ?
        g_mono_class_get_method_from_name(application_class, "get_Instance", 0) : 0;
    MonoMethod* enqueue = application_class ?
        g_mono_class_get_method_from_name(application_class, "EnqueueEventAction", 2) : 0;
    if (!application_class || !get_instance || !enqueue)
        return fail(25, "Application", "DISPATCH_METHOD_NOT_FOUND");

    MonoObject* exception = 0;
    MonoObject* application = g_mono_runtime_invoke(get_instance, 0, 0, &exception);
    if (!application || exception)
        return fail(26, "Application", "GET_INSTANCE_FAILED");

    MonoClass* layer = g_mono_class_from_name(
        app_image, "Sce.Vsh.ShellUI.AppSystem", "LayerManager");
    MonoMethod* find_scene = layer ?
        g_mono_class_get_method_from_name(layer, "FindContainerSceneByPath", 1) : 0;
    if (!layer || !find_scene)
        return fail(27, "LayerManager", "FIND_SCENE_NOT_FOUND");

    MonoDomain* current = g_mono_domain_get();
    if (!current) current = root;
    MonoString* game_path = g_mono_string_new(current, "Game");
    if (!game_path) return fail(28, "LayerManager", "GAME_STRING_FAILED");
    void* scene_args[1];
    scene_args[0] = game_path;
    exception = 0;
    MonoObject* game = g_mono_runtime_invoke(find_scene, 0, scene_args, &exception);
    if (!game || exception)
        return fail(29, "LayerManager", "GAME_SCENE_FAILED");

    MonoClass* scene_class = g_mono_class_from_name(pui, "Sce.PlayStation.PUI.UI2", "Scene");
    MonoProperty* root_prop = scene_class ?
        g_mono_class_get_property_from_name(scene_class, "RootWidget") : 0;
    MonoMethod* root_getter = root_prop ? g_mono_property_get_get_method(root_prop) : 0;
    exception = 0;
    MonoObject* root_widget = root_getter ?
        g_mono_runtime_invoke(root_getter, game, 0, &exception) : 0;
    if (!scene_class || !root_prop || !root_getter || !root_widget || exception)
        return fail(30, "Scene", "ROOT_WIDGET_FAILED");
    send_packet(KIND_STAGE, 0, 0, "PUI", "ROOT_WIDGET_READY");

    MonoClass* action_class = g_mono_class_from_name(corlib, "System", "Action");
    MonoClass* marshal_class = g_mono_class_from_name(
        corlib, "System.Runtime.InteropServices", "Marshal");
    MonoMethod* get_delegate = marshal_class ?
        g_mono_class_get_method_from_name(marshal_class, "GetDelegateForFunctionPointer", 2) : 0;
    MonoType* action_type = action_class ? g_mono_class_get_type(action_class) : 0;
    MonoObject* action_type_obj = action_type ? g_mono_type_get_object(root, action_type) : 0;
    if (!action_class || !marshal_class || !get_delegate || !action_type || !action_type_obj)
        return fail(31, "Marshal", "DELEGATE_FACTORY_NOT_FOUND");

    void* native_ptr = (void*)(uintptr_t)&native_queue_callback;
    void* delegate_args[2];
    delegate_args[0] = &native_ptr;
    delegate_args[1] = action_type_obj;
    exception = 0;
    MonoObject* action = g_mono_runtime_invoke(get_delegate, 0, delegate_args, &exception);
    if (!action || exception)
        return fail(32, "Marshal", "GET_DELEGATE_FAILED");

    MonoClass* actual_class = g_mono_object_get_class(action);
    const char* actual_name = actual_class ? g_mono_class_get_name(actual_class) : 0;
    if (!actual_name || strcmp(actual_name, "Action") != 0)
        return fail(33, "System.Action", "TYPE_VERIFY_FAILED");
    send_packet(KIND_STAGE, 0, 0, "System.Action", "NATIVE_DELEGATE_READY");

    __atomic_store_n((uint32_t*)&g_native_callback_seen, 0u, __ATOMIC_RELEASE);
    void* enqueue_args[2];
    enqueue_args[0] = root_widget;
    enqueue_args[1] = action;
    exception = 0;
    (void)g_mono_runtime_invoke(enqueue, application, enqueue_args, &exception);
    if (exception)
        return fail(34, "EnqueueEventAction", "INVOKE_EXCEPTION");
    send_packet(KIND_STAGE, 0, 0, "EnqueueEventAction", "NATIVE_ACTION_SUBMITTED");

    uint32_t seen = 0;
    for (unsigned i = 0; i < 300; ++i) {
        usleep(10000);
        seen = __atomic_load_n((uint32_t*)&g_native_callback_seen, __ATOMIC_ACQUIRE);
        if (seen == 1u) break;
    }

    send_packet(KIND_RESULT, 0, (int32_t)seen,
                "NATIVE_CALLBACK_SEEN", seen == 1u ? "ACTION_EXECUTED" : "ACTION_NOT_OBSERVED");
    if (seen != 1u)
        return fail(35, "NativeAction", "CALLBACK_EXECUTION_TIMEOUT");

    send_packet(KIND_DONE, 0, 1, "SR9G", "PASS_NATIVE_ACTION_EXECUTED_NO_UI_MUTATION");
    close(g_socket);
    return 0;
}
