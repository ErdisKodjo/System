/*
 * package_manager/package_manager_service.cpp — PMS.
 *
 * The PackageManagerService is the central registry of installed apps.
 * It is responsible for parsing APKs (delegated to PackageParser), storing
 * per-package metadata (package name, version, requested permissions,
 * activities / services / receivers declared in the manifest), granting
 * permissions (delegated to PermissionManager), and answering intent
 * queries ("which activity handles ACTION_VIEW for mime image/png?").
 *
 * In the sandbox, APK parsing is a stub — we accept the path, call
 * PackageParser to populate a PackageInfo-like struct, store it in the
 * package table, and return success. Permission grant/revoke/check is
 * delegated to permission_manager.cpp.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

extern "C" {
    /* PackageParser (package_parser.cpp). */
    int  PackageParserParse(const char *apk_path,
                            char *out_pkg, size_t pkg_max,
                            char *out_ver, size_t ver_max,
                            char *out_label, size_t label_max,
                            char *out_perms, size_t perms_max);
    /* PermissionManager (permission_manager.cpp). */
    int  PermissionGrant(const char *pkg, const char *perm);
    int  PermissionRevoke(const char *pkg, const char *perm);
    int  PermissionCheck(const char *pkg, const char *perm);
}

struct PackageInfo {
    std::string package_name;
    std::string version_name;
    std::string label;
    std::string apk_path;
    std::string data_dir;
    std::vector<std::string> requested_permissions;
    std::vector<std::string> activities;
    std::vector<std::string> services;
    std::vector<std::string> receivers;
    std::vector<std::string> providers;
    int         version_code;
    int         target_sdk;
    bool        system;
    bool        enabled;
};

class PackageManagerService {
public:
    status_t Install(const char *apk_path, const char *data_dir) {
        if (!apk_path) return BAD_VALUE;
        char pkg[256], ver[64], label[128], perms[1024];
        pkg[0] = ver[0] = label[0] = perms[0] = 0;
        int rc = PackageParserParse(apk_path, pkg, sizeof(pkg),
                                    ver, sizeof(ver),
                                    label, sizeof(label),
                                    perms, sizeof(perms));
        if (rc != OK) return rc;
        std::lock_guard<std::mutex> lk(mu_);
        if (pkgs_.find(pkg) != pkgs_.end()) return ALREADY_EXISTS;
        PackageInfo p;
        p.package_name = pkg;
        p.version_name = ver;
        p.label = label[0] ? label : pkg;
        p.apk_path = apk_path;
        p.data_dir = data_dir ? data_dir : "";
        p.version_code = 1;
        p.target_sdk = 30;
        p.system = false;
        p.enabled = true;
        /* Parse the comma-separated permission list. */
        const char *s = perms;
        while (*s) {
            const char *e = std::strchr(s, ',');
            std::string one = e ? std::string(s, e - s) : std::string(s);
            if (!one.empty()) {
                p.requested_permissions.push_back(one);
                PermissionGrant(p.package_name.c_str(), one.c_str());
            }
            if (!e) break;
            s = e + 1;
        }
        pkgs_[p.package_name] = std::move(p);
        return OK;
    }

    status_t Uninstall(const char *pkg) {
        if (!pkg) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        auto it = pkgs_.find(pkg);
        if (it == pkgs_.end()) return NAME_NOT_FOUND;
        pkgs_.erase(it);
        return OK;
    }

    /* Returns 1 if the package is installed, 0 otherwise. */
    int IsInstalled(const char *pkg) {
        if (!pkg) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        return pkgs_.find(pkg) != pkgs_.end() ? 1 : 0;
    }

    /* Resolve an intent (action + mime type) to a package that declares
     * an activity handling it. The sandbox matches on a simple convention:
     * a package whose name contains the action suffix is returned. */
    status_t ResolveActivity(const char *action, const char * /*mime*/,
                             char *out_pkg, size_t pkg_max) {
        if (!action) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &kv : pkgs_) {
            if (kv.first.find(action) != std::string::npos ||
                std::strstr(action, kv.first.c_str())) {
                std::strncpy(out_pkg, kv.first.c_str(), pkg_max - 1);
                out_pkg[pkg_max - 1] = 0;
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    status_t SetEnabled(const char *pkg, bool enabled) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = pkgs_.find(pkg);
        if (it == pkgs_.end()) return NAME_NOT_FOUND;
        it->second.enabled = enabled;
        return OK;
    }

    size_t InstalledCount() {
        std::lock_guard<std::mutex> lk(mu_);
        return pkgs_.size();
    }

    /* List installed package names into `out`; returns count. */
    size_t ListPackages(const char **out, size_t max) {
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = std::min(pkgs_.size(), max);
        size_t i = 0;
        for (auto &kv : pkgs_) {
            if (i >= n) break;
            out[i++] = kv.first.c_str();
        }
        return n;
    }

    int CheckPermission(const char *pkg, const char *perm) {
        return PermissionCheck(pkg, perm);
    }

private:
    std::mutex mu_;
    std::unordered_map<std::string, PackageInfo> pkgs_;
};

static PackageManagerService *g_pms = nullptr;
static PackageManagerService *pms() {
    if (!g_pms) g_pms = new PackageManagerService();
    return g_pms;
}

extern "C" {

status_t PmsInstall(const char *apk, const char *data_dir) {
    return pms()->Install(apk, data_dir);
}
status_t PmsUninstall(const char *pkg)              { return pms()->Uninstall(pkg); }
int      PmsIsInstalled(const char *pkg)            { return pms()->IsInstalled(pkg); }
status_t PmsResolveActivity(const char *action, const char *mime,
                            char *out, size_t max) {
    return pms()->ResolveActivity(action, mime, out, max);
}
status_t PmsSetEnabled(const char *pkg, int en)     { return pms()->SetEnabled(pkg, en != 0); }
size_t   PmsInstalledCount()                         { return pms()->InstalledCount(); }
size_t   PmsListPackages(const char **out, size_t m){ return pms()->ListPackages(out, m); }
int      PmsCheckPermission(const char *p, const char *perm) {
    return pms()->CheckPermission(p, perm);
}

} /* extern "C" */
