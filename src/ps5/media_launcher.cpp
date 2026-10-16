/* Common FPS for PS5 - Copyright (C) 2026 porhe911
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Media/deeplink registration reference: drakmor/ShadowMountPlus,
 * commit 98c524890176c7b474fcb03387bec88c4a559861 (GPLv3).
 * Common FPS implementation, 2026-10-15. */
#include "media_launcher.hpp"
#include "common_fps/session_log.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <ps5/kernel.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
extern const unsigned char commonfps_settings_param[];
extern const std::size_t commonfps_settings_param_size;
extern const unsigned char commonfps_settings_icon[];
extern const std::size_t commonfps_settings_icon_size;
}

namespace common_fps::ps5 {
namespace {
constexpr char kTitleId[] = "CFPS00060";
constexpr char kDir[] = "/user/app/CFPS00060";
constexpr char kSysDir[] = "/user/app/CFPS00060/sce_sys";
constexpr char kParam[] = "/user/app/CFPS00060/sce_sys/param.json";
constexpr std::uint64_t kAppInstallerAuth = 0x4800000000000006ULL;

bool directory(const char* path) {
    struct stat st{};
    if (lstat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return errno == ENOENT && mkdir(path, 0755) == 0;
}

bool asset(const char* path, const unsigned char* data, std::size_t size) {
    char temp[160]{};
    std::snprintf(temp, sizeof(temp), "%s.tmp", path);
    const int fd = open(temp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644);
    if (fd < 0) return false;
    std::size_t n = 0;
    while (n < size) {
        const auto written = write(fd, data+n, size-n);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        n += static_cast<std::size_t>(written);
    }
    bool ok = n == size && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temp, path) == 0;
    if (!ok) unlink(temp);
    return ok;
}

bool can_stage_owned_title() {
    struct stat st{};
    if (lstat(kParam, &st) != 0) return errno == ENOENT;
    if (!S_ISREG(st.st_mode) || st.st_size != static_cast<off_t>(commonfps_settings_param_size))
        return false;
    char data[1024]{};
    if (commonfps_settings_param_size > sizeof(data)) return false;
    FILE* f = fopen(kParam, "rb");
    if (!f) return false;
    const bool match = fread(data, 1, commonfps_settings_param_size, f) == commonfps_settings_param_size &&
        std::memcmp(data, commonfps_settings_param, commonfps_settings_param_size) == 0;
    fclose(f);
    return match;
}
} // namespace

bool ensure_media_launcher(std::uint32_t sdk_version) {
    // The upstream direct installer is unavailable on 12+. No ShellCore bridge.
    if ((sdk_version >> 24) >= 0x12) {
        log_durable_message(kControllerLogPath, "Media launcher skipped: direct installer unsupported on FW12+");
        return true;
    }
    void* module = dlopen("/system/common/lib/libSceAppInstUtil.sprx", RTLD_LAZY);
    using Init = int (*)();
    using Exists = int (*)(const char*, int*);
    using Install = int (*)(const char*, const char*, void*);
    const auto init = module ? reinterpret_cast<Init>(dlsym(module, "sceAppInstUtilInitialize")) : nullptr;
    const auto exists = module ? reinterpret_cast<Exists>(dlsym(module, "sceAppInstUtilAppExists")) : nullptr;
    const auto install = module ? reinterpret_cast<Install>(dlsym(module, "sceAppInstUtilAppInstallTitleDir")) : nullptr;
    if (!init || !exists || !install) {
        log_durable_message(kControllerLogPath, "Media launcher unavailable: AppInstUtil symbols missing");
        return true;
    }
    const auto original = kernel_get_ucred_authid(getpid());
    if (original == 0 || original == static_cast<std::uint64_t>(-1)) {
        log_durable_message(kControllerLogPath, "Media launcher skipped: cannot snapshot credentials");
        return true;
    }
    int init_rc = -1, check_rc = -1, install_rc = -1, present = 0;
    bool staged = false;
    if (kernel_set_ucred_authid(getpid(), kAppInstallerAuth) == 0 &&
        kernel_get_ucred_authid(getpid()) == kAppInstallerAuth) {
        init_rc = init();
        if (init_rc == 0) {
            check_rc = exists(kTitleId, &present);
            if (check_rc == 0 && !present && can_stage_owned_title() &&
                directory(kDir) && directory(kSysDir)) {
                staged = asset("/user/app/CFPS00060/sce_sys/icon0.png",
                               commonfps_settings_icon, commonfps_settings_icon_size) &&
                    asset(kParam, commonfps_settings_param, commonfps_settings_param_size);
                if (staged) install_rc = install(kTitleId, "/user/app/", nullptr);
                if (install_rc == 0 || static_cast<std::uint32_t>(install_rc) == 0x80990002U)
                    check_rc = exists(kTitleId, &present);
            }
        }
    }
    // Credential changes never overlap sampler work or the HTTP thread.
    const bool restored = kernel_set_ucred_authid(getpid(), original) == 0 &&
        kernel_get_ucred_authid(getpid()) == original;
    char record[256]{};
    std::snprintf(record, sizeof(record),
        "Media launcher title=%s init=0x%x check=0x%x staged=%d install=0x%x present=%d auth_restored=%d",
        kTitleId, init_rc, check_rc, staged ? 1 : 0, install_rc, present, restored ? 1 : 0);
    log_durable_message(kControllerLogPath, record);
    return restored;
}
} // namespace common_fps::ps5
