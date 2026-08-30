/*
 * Common FPS v0.28b SR9F - PUI EnqueueEventAction execution proof
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * NO UI mutation. The receiver obtains the already hardware-proven PUI context,
 * creates a managed ArrayList with one element, binds System.Action to
 * ArrayList.Clear(), then submits that action through
 * Application.EnqueueEventAction(RootWidget, Action). Execution is proven only
 * when ArrayList.Count changes from 1 to 0.
 */

#include <netinet/in.h>
#include <ps5/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LOOPBACK_ADDRESS 0x0100007fu
#define RESULT_PORT_NETWORK 0xFAD8u /* htons(55546) */
#define RESULT_MAGIC 0x46394551u      /* "QE9F" */

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
typedef MonoObject* (*mono_object_new_fn)(MonoDomain*, MonoClass*);
typedef void* (*mono_compile_method_fn)(MonoMethod*);
typedef void* (*mono_object_unbox_fn)(MonoObject*);

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
static mono_object_new_fn g_mono_object_new;
static mono_compile_method_fn g_mono_compile_method;
static mono_object_unbox_fn g_mono_object_unbox;

#define RESOLVE_MONO(handle, name, type) \
    do { g_##name = (type)(uintptr_t)kernel_dynlib_dlsym(getpid(), handle, #name); } while (0)

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

static int32_t invoke_int32(MonoMethod* method, MonoObject* instance, int* ok) {
    MonoObject* exception = 0;
    MonoObject* boxed = g_mono_runtime_invoke(method, instance, 0, &exception);
    if (exception || !boxed) {
        *ok = 0;
        return -1;
    }
    void* raw = g_mono_object_unbox(boxed);
    if (!raw) {
        *ok = 0;
        return -1;
    }
    *ok = 1;
    return *(int32_t*)raw;
}

int main(void) {
    g_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket < 0) return 10;
    memset(&g_destination, 0, sizeof(g_destination));
    g_destination.sin_family = AF_INET;
    g_destination.sin_port = RESULT_PORT_NETWORK;
    g_destination.sin_addr.s_addr = LOOPBACK_ADDRESS;
    send_packet(KIND_STAGE, 0, 0, "SR9F", "START_ENQUEUE_EXECUTION_PROOF");

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
    RESOLVE_MONO(mono_handle, mono_object_new, mono_object_new_fn);
    RESOLVE_MONO(mono_handle, mono_compile_method, mono_compile_method_fn);
    RESOLVE_MONO(mono_handle, mono_object_unbox, mono_object_unbox_fn);

    if (!g_mono_get_root_domain || !g_mono_thread_attach || !g_mono_domain_assembly_open ||
        !g_mono_assembly_get_image || !g_mono_get_corlib || !g_mono_class_from_name ||
        !g_mono_class_get_method_from_name || !g_mono_string_new || !g_mono_domain_get ||
        !g_mono_runtime_invoke || !g_mono_class_get_property_from_name ||
        !g_mono_property_get_get_method || !g_mono_object_new || !g_mono_compile_method ||
        !g_mono_object_unbox)
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

    MonoClass* list_class = g_mono_class_from_name(corlib, "System.Collections", "ArrayList");
    MonoMethod* list_ctor = list_class ? g_mono_class_get_method_from_name(list_class, ".ctor", 0) : 0;
    MonoMethod* list_add = list_class ? g_mono_class_get_method_from_name(list_class, "Add", 1) : 0;
    MonoMethod* list_clear = list_class ? g_mono_class_get_method_from_name(list_class, "Clear", 0) : 0;
    MonoMethod* list_count = list_class ? g_mono_class_get_method_from_name(list_class, "get_Count", 0) : 0;
    if (!list_class || !list_ctor || !list_add || !list_clear || !list_count)
        return fail(31, "ArrayList", "METHOD_NOT_FOUND");

    MonoObject* list = g_mono_object_new(root, list_class);
    if (!list) return fail(32, "ArrayList", "OBJECT_NEW_FAILED");
    exception = 0;
    (void)g_mono_runtime_invoke(list_ctor, list, 0, &exception);
    if (exception) return fail(33, "ArrayList", "CTOR_EXCEPTION");
    void* add_args[1];
    add_args[0] = application;
    exception = 0;
    (void)g_mono_runtime_invoke(list_add, list, add_args, &exception);
    if (exception) return fail(34, "ArrayList", "ADD_EXCEPTION");
    int count_ok = 0;
    int32_t before = invoke_int32(list_count, list, &count_ok);
    if (!count_ok || before != 1)
        return fail(35, "ArrayList", "PRECONDITION_COUNT_NOT_ONE");
    send_packet(KIND_RESULT, 0, before, "COUNT_BEFORE", "EXPECTED_1");

    MonoClass* action_class = g_mono_class_from_name(corlib, "System", "Action");
    MonoMethod* action_ctor = action_class ?
        g_mono_class_get_method_from_name(action_class, ".ctor", 2) : 0;
    if (!action_class || !action_ctor)
        return fail(36, "System.Action", "CTOR_NOT_FOUND");
    MonoObject* action = g_mono_object_new(root, action_class);
    if (!action) return fail(37, "System.Action", "OBJECT_NEW_FAILED");
    void* compiled_clear = g_mono_compile_method(list_clear);
    if (!compiled_clear) return fail(38, "ArrayList", "CLEAR_COMPILE_FAILED");
    void* ctor_args[2];
    ctor_args[0] = list;
    ctor_args[1] = &compiled_clear;
    exception = 0;
    (void)g_mono_runtime_invoke(action_ctor, action, ctor_args, &exception);
    if (exception) return fail(39, "System.Action", "CTOR_EXCEPTION");
    send_packet(KIND_STAGE, 0, 0, "System.Action", "DELEGATE_READY");

    void* enqueue_args[2];
    enqueue_args[0] = root_widget;
    enqueue_args[1] = action;
    exception = 0;
    (void)g_mono_runtime_invoke(enqueue, application, enqueue_args, &exception);
    if (exception)
        return fail(40, "EnqueueEventAction", "INVOKE_EXCEPTION");
    send_packet(KIND_STAGE, 0, 0, "EnqueueEventAction", "SUBMITTED");

    int32_t after = before;
    for (unsigned i = 0; i < 300; ++i) {
        usleep(10000);
        count_ok = 0;
        after = invoke_int32(list_count, list, &count_ok);
        if (!count_ok)
            return fail(41, "ArrayList", "COUNT_READ_FAILED");
        if (after == 0)
            break;
    }

    send_packet(KIND_RESULT, 0, after, "COUNT_AFTER", after == 0 ? "ACTION_EXECUTED" : "ACTION_NOT_OBSERVED");
    if (after != 0)
        return fail(42, "EnqueueEventAction", "ACTION_EXECUTION_TIMEOUT");

    send_packet(KIND_DONE, 0, 0, "SR9F", "PASS_UI_QUEUE_EXECUTED_NO_UI_MUTATION");
    close(g_socket);
    return 0;
}
