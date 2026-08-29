/*
 * Common FPS for PS5
 * Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commonfps_shellui.hpp"
#include "common_fps/renderer_health.hpp"

#include "Detour.h"
#include "HookedFuncs.hpp"
#include "defs.h"
#include "proc.h"
#include "ucred.h"

#include <atomic>
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

MonoImage* pui_img = nullptr;
MonoImage* AppSystem_img = nullptr;
MonoObject* Game = nullptr;

namespace {

std::atomic_bool g_visual_ready{false};
void (*g_update_orig)(MonoObject*) = nullptr;

void stage_log(const char* text) {
    FILE* f = std::fopen("/data/CommonFPS_stageB_shellui.log", "a");
    if (!f)
        return;
    std::fprintf(f, "%s\n", text);
    std::fclose(f);
}

int open_health_sender() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void send_health(int fd, common_fps::RendererHealthPhase phase, std::uint64_t seq) {
    if (fd < 0)
        return;

    common_fps::RendererHealthPacket packet{};
    packet.shellui_pid = static_cast<std::int32_t>(getpid());
    packet.phase = static_cast<std::uint16_t>(phase);
    packet.sequence = seq;

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(common_fps::kRendererHealthPort);
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    (void)sendto(fd, &packet, sizeof(packet), 0,
        reinterpret_cast<sockaddr*>(&target), sizeof(target));
}

bool resolve_mono_symbols() {
    const pid_t pid = getpid();
    const int mono_handle = get_module_handle(pid, "libmonosgen-2.0.sprx");
    if (mono_handle <= 0)
        return false;

    KERNEL_DLSYM(mono_handle, mono_get_root_domain);
    KERNEL_DLSYM(mono_handle, mono_property_get_get_method);
    KERNEL_DLSYM(mono_handle, mono_property_get_set_method);
    KERNEL_DLSYM(mono_handle, mono_class_get_property_from_name);
    KERNEL_DLSYM(mono_handle, mono_class_from_name);
    KERNEL_DLSYM(mono_handle, mono_runtime_invoke);
    KERNEL_DLSYM(mono_handle, mono_string_new);
    KERNEL_DLSYM(mono_handle, mono_object_new);
    KERNEL_DLSYM(mono_handle, mono_object_unbox);
    KERNEL_DLSYM(mono_handle, mono_compile_method);
    KERNEL_DLSYM(mono_handle, mono_assembly_get_image);
    KERNEL_DLSYM(mono_handle, mono_domain_assembly_open);
    KERNEL_DLSYM(mono_handle, mono_thread_attach);
    KERNEL_DLSYM(mono_handle, mono_class_get_method_from_name);
    KERNEL_DLSYM(mono_handle, mono_runtime_object_init);
    KERNEL_DLSYM(mono_handle, mono_domain_get);
    KERNEL_DLSYM(mono_handle, mono_object_get_class);
    KERNEL_DLSYM(mono_handle, mono_class_vtable);
    KERNEL_DLSYM(mono_handle, mono_vtable_get_static_field_data);
    KERNEL_DLSYM(mono_handle, mono_class_get_field_from_name);
    KERNEL_DLSYM(mono_handle, mono_field_static_set_value);
    KERNEL_DLSYM(mono_handle, mono_aot_get_method);

    return mono_get_root_domain &&
           mono_property_get_get_method &&
           mono_property_get_set_method &&
           mono_class_get_property_from_name &&
           mono_class_from_name &&
           mono_runtime_invoke && mono_string_new && mono_object_new &&
           mono_object_unbox && mono_compile_method &&
           mono_assembly_get_image && mono_domain_assembly_open &&
           mono_thread_attach && mono_class_get_method_from_name &&
           mono_runtime_object_init && mono_domain_get;
}

bool resolve_images() {
    pui_img = getDLLimage("Sce.PlayStation.PUI.dll");
    AppSystem_img = getDLLimage("Sce.Vsh.ShellUI.AppSystem.dll");
    return pui_img != nullptr && AppSystem_img != nullptr;
}

bool acquire_game_on_main_thread() {
    if (Game)
        return true;

    MonoClass* layer = mono_class_from_name(
        AppSystem_img,
        "Sce.Vsh.ShellUI.AppSystem",
        "LayerManager");
    if (!layer)
        return false;

    MonoMethod* find = mono_class_get_method_from_name(
        layer,
        "FindContainerSceneByPath",
        1);
    if (!find)
        return false;

    MonoString* path = mono_string_new(Root_Domain, "Game");
    void* args[1] = {path};
    MonoObject* exception = nullptr;
    MonoObject* candidate = mono_runtime_invoke(find, nullptr, args, &exception);
    if (exception || !candidate)
        return false;

    Game = candidate;
    return true;
}

void update_hook(MonoObject* instance) {
    if (!Game)
        (void)acquire_game_on_main_thread();

    if (Game) {
        common_fps::ps5::shellui::apply_latest_state();
        g_visual_ready.store(true);
    }

    if (g_update_orig)
        g_update_orig(instance);
}

bool install_update_hook() {
    const std::uint64_t address = Get_Address_of_Method(
        pui_img,
        "Sce.PlayStation.PUI",
        "Application",
        "Update",
        0);
    if (!address)
        return false;

    g_update_orig = reinterpret_cast<void (*)(MonoObject*)>(
        DetourFunction(address, reinterpret_cast<void*>(&update_hook)));
    return g_update_orig != nullptr;
}

} // namespace

int main(int, const char**) {
    using namespace common_fps::ps5::shellui;

    stage_log("R0 StageB renderer entered");

    if (!initialize_receiver()) {
        stage_log("R0 FAIL receiver");
        return 1;
    }

    const int health_fd = open_health_sender();
    std::uint64_t sequence = 1;
    send_health(health_fd, common_fps::RendererHealthPhase::ReceiverReady, sequence++);
    stage_log("R1 ReceiverReady");

    const std::uintptr_t old_authid = set_ucred_to_debugger();
    if (old_authid == 0) {
        stage_log("R2 FAIL debugger auth");
        return 2;
    }

    if (!resolve_mono_symbols()) {
        set_proc_authid(getpid(), old_authid);
        stage_log("R2 FAIL mono symbols");
        return 3;
    }

    Root_Domain = mono_get_root_domain();
    if (!Root_Domain) {
        set_proc_authid(getpid(), old_authid);
        stage_log("R2 FAIL root domain");
        return 4;
    }

    (void)mono_thread_attach(Root_Domain);
    stage_log("R2 Mono attached");

    /* Images can appear slightly after libmonosgen during ShellUI startup. */
    bool images_ready = false;
    for (unsigned i = 0; i < 120; ++i) {
        if (resolve_images()) {
            images_ready = true;
            break;
        }
        usleep(250000);
    }

    if (!images_ready) {
        set_proc_authid(getpid(), old_authid);
        stage_log("R3 FAIL PUI/AppSystem images");
        return 5;
    }
    stage_log("R3 PUI images ready");

    if (!install_update_hook()) {
        set_proc_authid(getpid(), old_authid);
        stage_log("R4 FAIL Application.Update detour");
        return 6;
    }

    set_proc_authid(getpid(), old_authid);
    stage_log("R4 Update hook installed; auth restored");

    bool visual_logged = false;
    for (;;) {
        const bool visual = g_visual_ready.load();
        send_health(
            health_fd,
            visual ? common_fps::RendererHealthPhase::VisualReady
                   : common_fps::RendererHealthPhase::ReceiverReady,
            sequence++);

        if (visual && !visual_logged) {
            stage_log("R5 VisualReady on ShellUI update thread");
            visual_logged = true;
        }
        sleep(1);
    }
}
