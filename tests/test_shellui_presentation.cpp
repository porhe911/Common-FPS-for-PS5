/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Compile the actual renderer, replacing only Mono/PUI with a moving-GC model.
// Unused PS5 bootstrap/network paths are discarded by --gc-sections.
#define COMMON_FPS_SHELLUI_HOST_TEST 1
#include "../src/ps5/shellui_payload/commonfps_shellui.cpp"

namespace common_fps::ps5::shellui {
namespace {
enum class Kind { Scene, Root, Label, Font, Color, String };
struct MonoDomain {};
struct MonoThread {};
struct MonoAssembly {};
struct MonoImage {};
struct MonoClass { Kind kind; };
struct MonoProperty { Kind kind; std::string name; };
struct MonoMethod { Kind kind; std::string name; };
struct MonoString {};
struct MonoObject {
    Kind kind{};
    bool alive = true;
    bool permanent = false;
    bool marked = false;
    int pins = 0;
    int size = 0;
    float x = 0, y = 0;
    std::string name, text;
    std::array<float,4> color{};
    alignas(16) std::array<unsigned char,32> payload{};
    std::vector<MonoObject*> children;
};

MonoDomain domain;
MonoImage image;
MonoThread thread;
MonoObject* scene = nullptr;
MonoObject* active_root = nullptr;
std::vector<std::unique_ptr<MonoObject>> heap;
std::map<std::uint32_t,MonoObject*> handles;
std::map<std::string,MonoProperty> properties;
std::map<std::string,MonoMethod> methods;
std::uint32_t next_handle = 1;
bool fail_pin = false, fail_color = false;
std::string last_stage;
unsigned font_allocations = 0, font_sets = 0, color_sets = 0, position_sets = 0;
unsigned dimension_queries = 0, collections = 0;

void mark(MonoObject* object) {
    if (!object || object->marked) return;
    assert(object->alive);
    object->marked = true;
    for (auto* child : object->children) mark(child);
}

void collect() {
    ++collections;
    for (const auto& object : heap) object->marked = false;
    for (const auto& object : heap)
        if (object->pins || object->permanent) mark(object.get());
    for (const auto& object : heap) if (!object->marked) object->alive = false;
}

MonoObject* allocate(Kind kind, bool permanent = false) {
    collect();
    auto object = std::make_unique<MonoObject>();
    object->kind = kind; object->permanent = permanent;
    heap.push_back(std::move(object));
    if (kind == Kind::Font) ++font_allocations;
    return heap.back().get();
}

void rooted(MonoObject* object) { assert(object && object->alive && object->pins > 0); }
MonoObject* owner(void* payload) {
    for (const auto& object : heap)
        if (object->payload.data() == payload) { rooted(object.get()); return object.get(); }
    assert(false && "Value type passed without its boxed owner");
    return nullptr;
}

MonoMethod* method(Kind kind, const std::string& name) {
    const auto key = std::to_string(static_cast<int>(kind))+name;
    return &methods.emplace(key,MonoMethod{kind,name}).first->second;
}

void set_name(MonoObject* object, MonoString* string) {
    collect(); rooted(object);
    auto* value = reinterpret_cast<MonoObject*>(string); rooted(value);
    object->name = value->text;
}
void set_text(MonoObject* object, MonoString* string) {
    collect(); rooted(object);
    auto* value = reinterpret_cast<MonoObject*>(string); rooted(value);
    object->text = value->text;
}
void set_x(MonoObject* object, float value) { collect(); rooted(object); object->x = value; ++position_sets; }
void set_y(MonoObject* object, float value) { collect(); rooted(object); object->y = value; ++position_sets; }
void set_int(MonoObject* object, int) { collect(); rooted(object); }
void set_bool(MonoObject* object, bool) { collect(); rooted(object); }
void font_ctor(void* value, int size, int, int) { collect(); auto* object = owner(value); object->size = size; }
void color_ctor(void* value, float r, float g, float b, float a) {
    collect(); auto* object = owner(value); object->color = {r,g,b,a};
}
MonoObject* find_child(MonoObject* root, MonoString* name) {
    collect(); rooted(root);
    auto* string = reinterpret_cast<MonoObject*>(name); rooted(string);
    for (auto* child : root->children) if (child->name == string->text) return child;
    return nullptr;
}

extern "C" {
MonoDomain* mono_get_root_domain() { return &domain; }
MonoDomain* mono_domain_get() { return &domain; }
MonoThread* mono_thread_attach(MonoDomain*) { return &thread; }
MonoAssembly* mono_domain_assembly_open(MonoDomain*, const char*) { return nullptr; }
MonoImage* mono_assembly_get_image(MonoAssembly*) { return &image; }
MonoClass* mono_class_from_name(MonoImage*, const char*, const char* name) {
    static MonoClass classes[] = {{Kind::Scene},{Kind::Root},{Kind::Label},{Kind::Font},{Kind::Color}};
    if (!std::strcmp(name,"Scene")) return &classes[0];
    if (!std::strcmp(name,"Widget")) return &classes[1];
    if (!std::strcmp(name,"Label")) return &classes[2];
    if (!std::strcmp(name,"UIFont")) return &classes[3];
    if (!std::strcmp(name,"UIColor")) return &classes[4];
    return nullptr;
}
MonoMethod* mono_class_get_method_from_name(MonoClass* klass, const char* name, int) {
    collect(); return method(klass->kind,name);
}
MonoProperty* mono_class_get_property_from_name(MonoClass* klass, const char* name) {
    collect();
    if (!std::strcmp(name,"Width") || !std::strcmp(name,"Height")) ++dimension_queries;
    const auto key = std::to_string(static_cast<int>(klass->kind))+name;
    return &properties.emplace(key,MonoProperty{klass->kind,name}).first->second;
}
MonoMethod* mono_property_get_get_method(MonoProperty* property) { return method(property->kind,"get_"+property->name); }
MonoMethod* mono_property_get_set_method(MonoProperty* property) { return method(property->kind,"set_"+property->name); }
MonoObject* mono_runtime_invoke(MonoMethod* m, void* instance, void** args, MonoObject** exception) {
    collect();
    if (exception) *exception = nullptr;
    if (m->name == "FindScene") { rooted(static_cast<MonoObject*>(args[0])); return scene; }
    auto* object = static_cast<MonoObject*>(instance); rooted(object);
    if (m->name == "get_RootWidget") return active_root;
    if (m->name == "AppendChild") {
        auto* child = static_cast<MonoObject*>(args[0]); rooted(child);
        object->children.push_back(child); return nullptr;
    }
    auto* value = owner(args[0]);
    if (m->name == "set_Font") {
        assert(value->kind == Kind::Font); object->size = value->size; ++font_sets;
    } else if (m->name == "set_TextColor") {
        if (fail_color) { fail_color = false; if (exception) *exception = scene; return nullptr; }
        assert(value->kind == Kind::Color); object->color = value->color; ++color_sets;
    } else assert(false && "Unexpected managed call");
    return nullptr;
}
MonoString* mono_string_new(MonoDomain*, const char* text) {
    auto* string = allocate(Kind::String); string->text = text;
    return reinterpret_cast<MonoString*>(string);
}
MonoObject* mono_object_new(MonoDomain*, MonoClass* klass) { return allocate(klass->kind); }
void mono_runtime_object_init(MonoObject* object) { collect(); rooted(object); }
void* mono_object_unbox(MonoObject* object) { collect(); rooted(object); return object->payload.data(); }
void* mono_compile_method(MonoMethod* m) {
    collect();
    if (m->name == ".ctor") return m->kind == Kind::Font ? reinterpret_cast<void*>(font_ctor) : reinterpret_cast<void*>(color_ctor);
    if (m->name == "FindWidgetByName") return reinterpret_cast<void*>(find_child);
    if (m->name == "set_Name") return reinterpret_cast<void*>(set_name);
    if (m->name == "set_Text") return reinterpret_cast<void*>(set_text);
    if (m->name == "set_X") return reinterpret_cast<void*>(set_x);
    if (m->name == "set_Y") return reinterpret_cast<void*>(set_y);
    if (m->name == "set_HorizontalAlignment" || m->name == "set_VerticalAlignment") return reinterpret_cast<void*>(set_int);
    if (m->name == "set_FitWidthToText" || m->name == "set_FitHeightToText") return reinterpret_cast<void*>(set_bool);
    assert(false && "Unexpected compiled thunk"); return nullptr;
}
std::uint32_t mono_gchandle_new(MonoObject* object, int pinned) {
    assert(object && object->alive && pinned == 1);
    if (fail_pin) { fail_pin = false; return 0; }
    ++object->pins; handles[next_handle] = object; return next_handle++;
}
MonoObject* mono_gchandle_get_target(std::uint32_t handle) {
    const auto it = handles.find(handle); return it == handles.end() ? nullptr : it->second;
}
void mono_gchandle_free(std::uint32_t handle) {
    auto* object = handles.at(handle); assert(object->pins > 0); --object->pins; handles.erase(handle);
}
} // extern C
} // namespace

void record_stage(const char* stage) noexcept { last_stage = stage; }
} // namespace common_fps::ps5::shellui

using namespace common_fps;
using namespace common_fps::ps5::shellui;

static void present(OverlayFrame frame, std::uint64_t sequence) {
    assert(apply_packet_on_render(make_wire_packet(frame,sequence)));
    assert(managed_target(g_label_handle)->alive && managed_target(g_value_handle)->alive);
    assert(handles.size() == 4); // Scene, root and the two labels; no leaked boxes.
}

int main() {
    scene = allocate(Kind::Scene,true);
    active_root = allocate(Kind::Root,true);
    g_domain = &domain; g_pui_image = &image; g_find_scene_method = method(Kind::Scene,"FindScene");
    OverlayFrame frame; frame.loading = false; frame.fps = 60;
    frame.config.corner = Corner::TopRight;
    frame.config.label_color = 0x00FF00; frame.config.value_color = 0xFF0000;
    present(frame,1);
    assert(font_allocations == 1 && font_sets == 2 && color_sets == 2);
    assert(managed_target(g_label_handle)->color[1] == 1);
    assert(managed_target(g_value_handle)->color[0] == 1);
    assert(managed_target(g_value_handle)->text == "60");
    const auto positions = position_sets;
    for (unsigned i = 2; i < 100; ++i) { frame.fps = 30+i%31; present(frame,i); }
    assert(font_allocations == 1 && font_sets == 2 && color_sets == 2 && position_sets == positions);
    frame.config.label_color = 0x0000FF;
    present(frame,100);
    assert(font_allocations == 1 && font_sets == 2 && color_sets == 3);
    frame.config.corner = Corner::BottomRight;
    present(frame,101);
    assert(font_allocations == 1 && font_sets == 2 && color_sets == 3);
    frame.config.font_size = 40;
    present(frame,102);
    assert(font_allocations == 2 && font_sets == 4);
    assert(managed_target(g_value_handle)->size == 40);
    frame.config.show_label = false;
    present(frame,103);
    assert(managed_target(g_label_handle)->text.empty());
    assert(managed_target(g_value_handle)->x == g_applied_layout.anchor.x);
    auto* old_root = active_root;
    auto* old_label = managed_target(g_label_handle);
    active_root = allocate(Kind::Root,true); old_root->permanent = false;
    present(frame,104);
    assert(managed_target(g_label_handle) != old_label && active_root->children.size() == 2);
    collect(); assert(!old_root->alive && !old_label->alive);
    assert(font_allocations == 3 && font_sets == 6);
    frame.config.value_color = 0xFFFF00; fail_color = true;
    assert(!apply_packet_on_render(make_wire_packet(frame,105)));
    assert(!g_style_applied && last_stage == "ui_style_failed");
    present(frame,106); assert(last_stage == "ui_style_ready");
    // Failed replacement must retain the old valid root handle.
    auto* other = allocate(Kind::Root,true);
    const auto saved = g_root_handle; fail_pin = true;
    assert(!replace_managed_handle(g_root_handle,other));
    assert(g_root_handle == saved && managed_target(saved) == active_root);
    other->permanent = false;
    for (unsigned i = 107; i < 300; ++i) {
        frame.config.label_color = i%2 ? 0x123456 : 0xFEDCBA;
        frame.config.corner = i%2 ? Corner::TopLeft : Corner::BottomRight;
        present(frame,i);
    }
    assert(dimension_queries == 0 && collections > 1000);
    release_managed_handle(g_label_handle); release_managed_handle(g_value_handle);
    release_managed_handle(g_root_handle); release_managed_handle(g_game_scene_handle);
    assert(handles.empty());
    std::cout << "Actual renderer: pinned value lifetimes, selective updates, root replacement and failure recovery PASS\n";
}
