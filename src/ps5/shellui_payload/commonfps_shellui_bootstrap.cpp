/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui_bootstrap.hpp"
#include "commonfps_shellui.hpp"

#include "Detour.h"
#include "HookedFuncs.hpp"
#include "defs.h"
#include "proc.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <machine/param.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * These globals are part of the small etaHEN GPL ShellUI support surface used
 * by MonoUtils.cpp / Detour.cpp.  The full etaHEN prx.cpp is intentionally not
 * linked into Common FPS.
 */
MonoImage* pui_img = nullptr;
MonoImage* AppSystem_img = nullptr;
MonoObject* Game = nullptr;

bool has_hv_bypass = false;
bool is_testkit = false;

namespace common_fps::ps5::shellui {
namespace {

using UpdateFn = void (*)(MonoObject* instance);

std::atomic_bool g_hook_installed{false};
std::atomic_bool g_visual_ready{false};
UpdateFn g_update_original = nullptr;
MonoThread* g_bootstrap_thread = nullptr;

constexpr std::uint8_t kAbsoluteIndirectJumpPrefix[6] = {
    0xFF, 0x25, 0x00, 0x00, 0x00, 0x00
};

bool resolve_mono_symbols()
{
    const int mono_handle =
        get_module_handle(getpid(), "libmonosgen-2.0.sprx");

    if (mono_handle <= 0)
        return false;

    /* Minimal symbol set required by bootstrap + Common FPS PUI helpers. */
    KERNEL_DLSYM(mono_handle, mono_get_root_domain);
    KERNEL_DLSYM(mono_handle, mono_domain_assembly_open);
    KERNEL_DLSYM(mono_handle, mono_assembly_get_image);
    KERNEL_DLSYM(mono_handle, mono_thread_attach);
    KERNEL_DLSYM(mono_handle, mono_class_from_name);
    KERNEL_DLSYM(mono_handle, mono_class_get_method_from_name);
    KERNEL_DLSYM(mono_handle, mono_runtime_invoke);
    KERNEL_DLSYM(mono_handle, mono_string_new);
    KERNEL_DLSYM(mono_handle, mono_compile_method);
    KERNEL_DLSYM(mono_handle, mono_class_get_property_from_name);
    KERNEL_DLSYM(mono_handle, mono_property_get_get_method);
    KERNEL_DLSYM(mono_handle, mono_property_get_set_method);
    KERNEL_DLSYM(mono_handle, mono_object_new);
    KERNEL_DLSYM(mono_handle, mono_object_unbox);
    KERNEL_DLSYM(mono_handle, mono_runtime_object_init);

    return
        mono_get_root_domain != nullptr &&
        mono_domain_assembly_open != nullptr &&
        mono_assembly_get_image != nullptr &&
        mono_thread_attach != nullptr &&
        mono_class_from_name != nullptr &&
        mono_class_get_method_from_name != nullptr &&
        mono_runtime_invoke != nullptr &&
        mono_string_new != nullptr &&
        mono_compile_method != nullptr &&
        mono_class_get_property_from_name != nullptr &&
        mono_property_get_get_method != nullptr &&
        mono_property_get_set_method != nullptr &&
        mono_object_new != nullptr &&
        mono_object_unbox != nullptr &&
        mono_runtime_object_init != nullptr;
}

MonoImage* open_system_image(const char* dll_name)
{
    if (!Root_Domain || !dll_name)
        return nullptr;

    char path[256]{};
    const int n = std::snprintf(
        path,
        sizeof(path),
        "/system_ex/common_ex/lib/%s",
        dll_name);

    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(path))
        return nullptr;

    MonoAssembly* assembly =
        mono_domain_assembly_open(Root_Domain, path);

    if (!assembly)
        return nullptr;

    return mono_assembly_get_image(assembly);
}

bool locate_game_scene()
{
    if (!AppSystem_img) {
        AppSystem_img =
            open_system_image("Sce.Vsh.ShellUI.AppSystem.dll");
    }

    if (!pui_img)
        pui_img = open_system_image("Sce.PlayStation.PUI.dll");

    if (!AppSystem_img || !pui_img)
        return false;

    MonoClass* layer_manager =
        mono_class_from_name(
            AppSystem_img,
            "Sce.Vsh.ShellUI.AppSystem",
            "LayerManager");

    if (!layer_manager)
        return false;

    MonoMethod* find_scene =
        mono_class_get_method_from_name(
            layer_manager,
            "FindContainerSceneByPath",
            1);

    if (!find_scene)
        return false;

    MonoString* path = mono_string_new(Root_Domain, "Game");
    if (!path)
        return false;

    void* args[1] = {path};
    MonoObject* exception = nullptr;

    MonoObject* game =
        mono_runtime_invoke(find_scene, nullptr, args, &exception);

    if (exception || !game)
        return false;

    Game = game;
    return true;
}

bool game_root_ready()
{
    if (!pui_img || !Game)
        return false;

    MonoObject* root =
        Get_Property<MonoObject*>(
            pui_img,
            "Sce.PlayStation.PUI.UI2",
            "Scene",
            Game,
            "RootWidget");

    return root != nullptr;
}

void application_update_hook(MonoObject* instance)
{
    /*
     * Preserve an existing etaHEN render hook first.  etaHEN 2.4B already
     * detours this exact Application.Update method, so Common FPS must chain
     * through it rather than replacing/bypassing it.
     */
    UpdateFn original = g_update_original;
    if (original)
        original(instance);

    /*
     * This callback is our only PUI execution point.  PUI mutation remains on
     * the real ShellUI Application.Update thread.
     */
    if (!g_visual_ready.load(std::memory_order_relaxed) && game_root_ready())
        g_visual_ready.store(true, std::memory_order_release);

    if (g_visual_ready.load(std::memory_order_acquire))
        apply_latest_state();
}

bool decode_existing_absolute_jump(
    std::uint64_t address,
    std::uint64_t* destination)
{
    if (!address || !destination)
        return false;

    const auto* code =
        reinterpret_cast<const std::uint8_t*>(address);

    if (std::memcmp(
            code,
            kAbsoluteIndirectJumpPrefix,
            sizeof(kAbsoluteIndirectJumpPrefix)) != 0) {
        return false;
    }

    std::uint64_t target = 0;
    std::memcpy(&target, code + 6, sizeof(target));
    if (!target)
        return false;

    *destination = target;
    return true;
}

bool make_code_page_writable(std::uint64_t address)
{
    const std::uint64_t page =
        address & ~(static_cast<std::uint64_t>(PAGE_SIZE) - 1ULL);

    return kernel_mprotect(
               getpid(),
               page,
               PAGE_SIZE,
               PROT_EXEC | PROT_READ | PROT_WRITE) == 0;
}

bool chain_over_existing_hook(
    std::uint64_t update_address,
    std::uint64_t existing_destination)
{
    if (!update_address || !existing_destination)
        return false;

    const auto ours =
        reinterpret_cast<std::uint64_t>(&application_update_hook);

    if (existing_destination == ours) {
        g_hook_installed.store(true, std::memory_order_release);
        return true;
    }

    if (!make_code_page_writable(update_address))
        return false;

    /*
     * etaHEN's WriteJump layout is:
     *   FF 25 00 00 00 00
     *   <8-byte absolute destination>
     *
     * Do not run DetourFunction over that already-detoured 14-byte sequence.
     * Keep its opcode intact, remember etaHEN's destination, and only replace
     * the destination pointer with Common FPS. The call chain becomes:
     *
     * Application.Update -> Common FPS -> etaHEN OnRender_Hook
     *                    -> etaHEN trampoline -> original Update
     */
    g_update_original =
        reinterpret_cast<UpdateFn>(existing_destination);

    std::memcpy(
        reinterpret_cast<void*>(update_address + 6),
        &ours,
        sizeof(ours));

    __builtin___clear_cache(
        reinterpret_cast<char*>(update_address),
        reinterpret_cast<char*>(update_address + 14));

    std::uint64_t verify = 0;
    if (!decode_existing_absolute_jump(update_address, &verify) ||
        verify != ours) {
        std::memcpy(
            reinterpret_cast<void*>(update_address + 6),
            &existing_destination,
            sizeof(existing_destination));
        g_update_original = nullptr;
        return false;
    }

    g_hook_installed.store(true, std::memory_order_release);
    return true;
}

bool install_application_update_hook()
{
    if (g_hook_installed.load(std::memory_order_acquire))
        return true;

    if (!pui_img || !Game)
        return false;

    const std::uint64_t update_address =
        Get_Address_of_Method(
            pui_img,
            "Sce.PlayStation.PUI",
            "Application",
            "Update",
            0);

    if (!update_address)
        return false;

    /*
     * etaHEN 2.4B already hooks the same method with its FF25 absolute jump.
     * Chain safely when that hook is present. Standalone ELF mode, where no
     * hook is present yet, falls back to the ordinary source DetourFunction.
     */
    std::uint64_t existing_destination = 0;
    if (decode_existing_absolute_jump(
            update_address,
            &existing_destination)) {
        return chain_over_existing_hook(
            update_address,
            existing_destination);
    }

    void* trampoline =
        DetourFunction(
            update_address,
            reinterpret_cast<void*>(&application_update_hook));

    if (!trampoline)
        return false;

    g_update_original = reinterpret_cast<UpdateFn>(trampoline);
    g_hook_installed.store(true, std::memory_order_release);
    return true;
}

} // namespace

bool initialize_visual_hook()
{
    if (g_hook_installed.load(std::memory_order_acquire))
        return true;

    if (!resolve_mono_symbols())
        return false;

    if (!Root_Domain)
        Root_Domain = mono_get_root_domain();

    if (!Root_Domain)
        return false;

    if (!g_bootstrap_thread)
        g_bootstrap_thread = mono_thread_attach(Root_Domain);

    if (!g_bootstrap_thread)
        return false;

    if (!locate_game_scene())
        return false;

    return install_application_update_hook();
}

bool visual_ready()
{
    return g_visual_ready.load(std::memory_order_acquire);
}

} // namespace common_fps::ps5::shellui
