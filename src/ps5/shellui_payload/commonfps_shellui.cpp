/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Source-only ShellUI renderer.
 *
 * Stage 8.2 keeps the source-built PUI renderer, but never patches a native
 * Sony method while SceShellUI is running. Native prologues are decoded and
 * relocated here, then a checked request asks the controller to stop
 * SceShellUI and apply the 16-byte patch through MDBG. The hardware-proven
 * etaHEN chain remains an in-process eight-byte destination update.
 */

#include "commonfps_shellui.hpp"
#include "common_fps/shellui_hook_protocol.hpp"

#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <cstdarg>
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
bool g_hook_install_confirmed = false;

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
 * Stage 8.2 uses two deliberately separate paths:
 *
 *   1. etaHEN's existing 14-byte absolute jump is chained exactly like the
 *      hardware-proven v1.1.0 renderer;
 *   2. a native, position-independent Sony prologue is never changed here.
 *      A request is published for a stopped-process MDBG patch instead.
 */
extern "C" __attribute__((naked, noinline, used, aligned(16)))
void commonfps_update_trampoline() {
    __asm__ volatile(
        ".rept 128\n"
        "nop\n"
        ".endr\n"
        "ret\n");
}

void application_update_hook(MonoObject* instance);

constexpr std::size_t kAbsoluteJumpSize = 14;
constexpr std::size_t kAtomicPatchSize = 16;
constexpr std::size_t kTrampolineCapacity = 128;

void encode_absolute_jump(
    std::uint8_t* output,
    const void* destination) noexcept {

    static constexpr std::uint8_t kPrefix[6] = {
        0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
    };
    std::memcpy(output, kPrefix, sizeof(kPrefix));
    const std::uint64_t target =
        reinterpret_cast<std::uint64_t>(destination);
    std::memcpy(output + sizeof(kPrefix), &target, sizeof(target));
}

bool prepare_trampoline(
    const std::uint8_t* original,
    std::size_t original_size,
    const void* continuation) noexcept {

    if (!original || original_size == 0 ||
        original_size + (continuation ? kAbsoluteJumpSize : 0) >
            kTrampolineCapacity) {
        return false;
    }

    auto* trampoline = reinterpret_cast<std::uint8_t*>(
        &commonfps_update_trampoline);
    std::memcpy(trampoline, original, original_size);
    std::size_t written = original_size;
    if (continuation) {
        encode_absolute_jump(trampoline + written, continuation);
        written += kAbsoluteJumpSize;
    }

    __builtin___clear_cache(
        reinterpret_cast<char*>(trampoline),
        reinterpret_cast<char*>(trampoline + written));
    return true;
}

std::size_t decode_relocatable_instruction(
    const std::uint8_t* code,
    std::size_t available) noexcept {

    if (!code || available == 0)
        return 0;

    std::size_t cursor = 0;
    bool rex_w = false;
    bool operand16 = false;

    while (cursor < available) {
        const std::uint8_t byte = code[cursor];
        if (byte == 0x66) {
            operand16 = true;
            ++cursor;
            continue;
        }
        if (byte == 0x67) {
            /* Address-size overrides complicate safe relocation. */
            return 0;
        }
        if (byte == 0xf0 || byte == 0xf2 || byte == 0xf3 ||
            byte == 0x2e || byte == 0x36 || byte == 0x3e ||
            byte == 0x26 || byte == 0x64 || byte == 0x65) {
            ++cursor;
            continue;
        }
        if (byte >= 0x40 && byte <= 0x4f) {
            rex_w = (byte & 0x08U) != 0;
            ++cursor;
            continue;
        }
        break;
    }

    if (cursor >= available)
        return 0;

    const std::uint8_t opcode = code[cursor++];

    if ((opcode >= 0x50 && opcode <= 0x5f) || opcode == 0x90)
        return cursor;

    if (opcode == 0x68) {
        const std::size_t immediate = operand16 ? 2U : 4U;
        return cursor + immediate <= available
            ? cursor + immediate
            : 0;
    }
    if (opcode == 0x6a)
        return cursor + 1U <= available ? cursor + 1U : 0;

    if (opcode >= 0xb8 && opcode <= 0xbf) {
        const std::size_t immediate = rex_w ? 8U : (operand16 ? 2U : 4U);
        return cursor + immediate <= available
            ? cursor + immediate
            : 0;
    }

    /* Never relocate relative control flow. */
    if (opcode == 0xe8 || opcode == 0xe9 || opcode == 0xeb ||
        (opcode >= 0x70 && opcode <= 0x7f) ||
        (opcode >= 0xe0 && opcode <= 0xe3)) {
        return 0;
    }

    std::size_t immediate = 0;
    bool has_modrm = false;

    switch (opcode) {
    case 0x01: case 0x03: case 0x09: case 0x0b:
    case 0x21: case 0x23: case 0x29: case 0x2b:
    case 0x31: case 0x33: case 0x39: case 0x3b:
    case 0x63: case 0x85: case 0x87: case 0x89:
    case 0x8b: case 0x8d:
        has_modrm = true;
        break;
    case 0x80: case 0x82: case 0x83: case 0xc6:
        has_modrm = true;
        immediate = 1;
        break;
    case 0x81: case 0xc7:
        has_modrm = true;
        immediate = operand16 ? 2U : 4U;
        break;
    case 0x0f: {
        if (cursor >= available)
            return 0;
        const std::uint8_t second = code[cursor++];
        if (second >= 0x80 && second <= 0x8f)
            return 0;
        if (second == 0x1e || second == 0x1f ||
            second == 0xb6 || second == 0xb7 ||
            second == 0xbe || second == 0xbf) {
            has_modrm = true;
            break;
        }
        return 0;
    }
    default:
        return 0;
    }

    if (!has_modrm || cursor >= available)
        return 0;

    const std::uint8_t modrm = code[cursor++];
    const std::uint8_t mod = static_cast<std::uint8_t>(modrm >> 6);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 7U);

    if (mod != 3 && rm == 4) {
        if (cursor >= available)
            return 0;
        const std::uint8_t sib = code[cursor++];
        const std::uint8_t base = static_cast<std::uint8_t>(sib & 7U);
        if (mod == 0 && base == 5) {
            if (cursor + 4U > available)
                return 0;
            cursor += 4U;
        }
    } else if (mod == 0 && rm == 5) {
        /* RIP-relative data access cannot be copied verbatim. */
        return 0;
    }

    if (mod == 1) {
        if (cursor + 1U > available)
            return 0;
        cursor += 1U;
    } else if (mod == 2) {
        if (cursor + 4U > available)
            return 0;
        cursor += 4U;
    }

    if (cursor + immediate > available)
        return 0;
    return cursor + immediate;
}

std::size_t native_patch_length(
    const std::uint8_t* address) noexcept {

    std::size_t length = 0;
    while (length < kAbsoluteJumpSize) {
        const std::size_t decoded = decode_relocatable_instruction(
            address + length,
            kAtomicPatchSize - length);
        if (decoded == 0)
            return 0;
        length += decoded;
    }

    return length <= kAtomicPatchSize ? length : 0;
}

template <typename T>
bool write_exact_file(
    const char* path,
    const T& value) noexcept {

    FILE* fp = std::fopen(path, "wb");
    if (!fp)
        return false;

    const bool complete =
        std::fwrite(&value, 1, sizeof(value), fp) == sizeof(value) &&
        std::fflush(fp) == 0;
    const bool closed = std::fclose(fp) == 0;
    return complete && closed;
}

template <typename T>
bool read_exact_file(const char* path, T& value) noexcept {
    FILE* fp = std::fopen(path, "rb");
    if (!fp)
        return false;

    const std::size_t count = std::fread(&value, 1, sizeof(value), fp);
    std::fclose(fp);
    return count == sizeof(value);
}

bool publish_native_hook_request(
    std::uint8_t* method,
    const std::array<std::uint8_t, kAtomicPatchSize>& expected,
    const std::array<std::uint8_t, kAtomicPatchSize>& desired,
    std::size_t displaced_size,
    std::uint64_t& nonce) noexcept {

    ShellUiHookRequest request{};
    request.pid = getpid();
    request.method_address =
        reinterpret_cast<std::uint64_t>(method);
    request.hook_address =
        reinterpret_cast<std::uint64_t>(&application_update_hook);
    request.trampoline_address =
        reinterpret_cast<std::uint64_t>(&commonfps_update_trampoline);
    request.displaced_size = static_cast<std::uint32_t>(displaced_size);
    std::memcpy(request.expected, expected.data(), expected.size());
    std::memcpy(request.desired, desired.data(), desired.size());
    request.nonce = request.method_address ^ request.hook_address ^
        request.trampoline_address ^
        (static_cast<std::uint64_t>(request.pid) << 32U) ^
        0x8d4f23a76c19e502ULL;
    if (request.nonce == 0)
        request.nonce = 1;
    request.checksum = shellui_hook_request_checksum(request);
    nonce = request.nonce;

    /* The controller ignores short/checksum-invalid in-progress reads. */
    return write_exact_file(
        kShellUiHookRequestPath,
        request);
}

bool wait_for_native_hook_ack(
    pid_t pid,
    std::uint64_t nonce,
    ShellUiHookAck& ack) noexcept {

    for (int attempt = 0; attempt < 1000; ++attempt) {
        ShellUiHookAck candidate{};
        if (read_exact_file(kShellUiHookAckPath, candidate) &&
            candidate.magic == kShellUiHookAckMagic &&
            candidate.version == kShellUiHookProtocolVersion &&
            candidate.pid == pid &&
            candidate.nonce == nonce &&
            candidate.checksum == shellui_hook_ack_checksum(candidate)) {
            ack = candidate;
            return true;
        }
        usleep(20000);
    }
    return false;
}

void log_line(const char* fmt, ...) {
    FILE* fp = std::fopen(
        "/data/CommonFPS_universal_stage8_2_shellui.log", "a");
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

bool install_update_hook(MonoClass* application_class) {
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

    std::array<std::uint8_t, kAtomicPatchSize> expected{};
    std::memcpy(expected.data(), address, expected.size());

    static constexpr std::uint8_t kAbsoluteJumpPrefix[6] = {
        0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
    };

    const bool eta_chain =
        std::memcmp(
            expected.data(),
            kAbsoluteJumpPrefix,
            sizeof(kAbsoluteJumpPrefix)) == 0;

    if (eta_chain) {
        std::uint64_t previous_destination = 0;
        std::memcpy(
            &previous_destination,
            expected.data() + sizeof(kAbsoluteJumpPrefix),
            sizeof(previous_destination));

        if (previous_destination ==
            reinterpret_cast<std::uint64_t>(&application_update_hook)) {
            if (g_application_update_original &&
                g_hook_install_confirmed) {
                log_line(
                    "Application.Update hook already online method=%p",
                    static_cast<void*>(address));
                return true;
            }
            log_line(
                "Application.Update belongs to another resident renderer "
                "method=%p",
                static_cast<void*>(address));
            return false;
        }

        if (previous_destination < 0x10000ULL) {
            log_line(
                "Application.Update etaHEN destination invalid method=%p "
                "destination=%p",
                static_cast<void*>(address),
                reinterpret_cast<void*>(previous_destination));
            return false;
        }

        if (!prepare_trampoline(
                expected.data(),
                kAbsoluteJumpSize,
                nullptr)) {
            log_line(
                "Application.Update trampoline prepare failed method=%p "
                "mode=etahen-chain displaced=%zu",
                static_cast<void*>(address),
                kAbsoluteJumpSize);
            return false;
        }

        g_application_update_original =
            reinterpret_cast<application_update_t>(
                &commonfps_update_trampoline);
        const std::uint64_t destination =
            reinterpret_cast<std::uint64_t>(&application_update_hook);
        std::memcpy(
            address + sizeof(kAbsoluteJumpPrefix),
            &destination,
            sizeof(destination));
        __builtin___clear_cache(
            reinterpret_cast<char*>(address),
            reinterpret_cast<char*>(address + kAbsoluteJumpSize));

        log_line(
            "Application.Update hook online method=%p mode=etahen-chain "
            "previous=%p hook=%p displaced=%zu stopped_patch=0",
            static_cast<void*>(address),
            reinterpret_cast<void*>(previous_destination),
            reinterpret_cast<void*>(&application_update_hook),
            kAbsoluteJumpSize);
        g_hook_install_confirmed = true;
        return true;
    }

    const std::size_t displaced_size = native_patch_length(address);
    if (displaced_size == 0) {
        log_line(
            "Application.Update native prologue rejected "
            "method=%p bytes="
            "%02x%02x%02x%02x%02x%02x%02x%02x"
            "%02x%02x%02x%02x%02x%02x%02x%02x",
            static_cast<void*>(address),
            expected[0], expected[1], expected[2], expected[3],
            expected[4], expected[5], expected[6], expected[7],
            expected[8], expected[9], expected[10], expected[11],
            expected[12], expected[13], expected[14], expected[15]);
        return false;
    }

    if (!prepare_trampoline(
            expected.data(),
            displaced_size,
            address + displaced_size)) {
        log_line(
            "Application.Update trampoline prepare failed method=%p "
            "mode=native-stopped-mdbg displaced=%zu",
            static_cast<void*>(address),
            displaced_size);
        return false;
    }

    std::array<std::uint8_t, kAtomicPatchSize> desired = expected;
    encode_absolute_jump(
        desired.data(),
        reinterpret_cast<const void*>(&application_update_hook));
    for (std::size_t i = kAbsoluteJumpSize;
         i < displaced_size;
         ++i) {
        desired[i] = 0x90;
    }

    /*
     * Publish the original target before the controller can resume ShellUI.
     * The new hook may execute immediately after PT_DETACH, before this
     * renderer thread observes the acknowledgement.
     */
    g_application_update_original =
        reinterpret_cast<application_update_t>(
            &commonfps_update_trampoline);

    std::uint64_t nonce = 0;
    if (!publish_native_hook_request(
            address,
            expected,
            desired,
            displaced_size,
            nonce)) {
        log_line(
            "Application.Update native request publish failed method=%p",
            static_cast<void*>(address));
        return false;
    }

    log_line(
        "Application.Update native request ready method=%p hook=%p "
        "trampoline=%p displaced=%zu nonce=0x%llx",
        static_cast<void*>(address),
        reinterpret_cast<void*>(&application_update_hook),
        reinterpret_cast<void*>(&commonfps_update_trampoline),
        displaced_size,
        static_cast<unsigned long long>(nonce));

    ShellUiHookAck ack{};
    if (!wait_for_native_hook_ack(getpid(), nonce, ack)) {
        log_line(
            "Application.Update native request timeout method=%p "
            "nonce=0x%llx",
            static_cast<void*>(address),
            static_cast<unsigned long long>(nonce));
        return false;
    }

    if (ack.status != static_cast<std::int32_t>(
            ShellUiHookStatus::Success) ||
        ack.verified == 0 || ack.detached == 0 ||
        ack.auth_restored == 0) {
        log_line(
            "Application.Update native request failed method=%p "
            "status=%d read_rc=%d write_rc=%d verified=%u "
            "restored=%u detached=%u auth_restored=%u",
            static_cast<void*>(address),
            ack.status,
            ack.read_rc,
            ack.write_rc,
            static_cast<unsigned>(ack.verified),
            static_cast<unsigned>(ack.restored),
            static_cast<unsigned>(ack.detached),
            static_cast<unsigned>(ack.auth_restored));
        return false;
    }

    if (std::memcmp(address, desired.data(), desired.size()) != 0) {
        log_line(
            "Application.Update native readback changed method=%p",
            static_cast<void*>(address));
        return false;
    }
    __builtin___clear_cache(
        reinterpret_cast<char*>(address),
        reinterpret_cast<char*>(address + desired.size()));

    log_line(
        "Application.Update hook online method=%p "
        "mode=native-stopped-mdbg previous=%p hook=%p "
        "displaced=%zu stopped_patch=1 verified=1",
        static_cast<void*>(address),
        nullptr,
        reinterpret_cast<void*>(&application_update_hook),
        displaced_size);
    g_hook_install_confirmed = true;
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
    if (!application_class || !install_update_hook(application_class)) {
        log_line("Application.Update universal hook unavailable");
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
