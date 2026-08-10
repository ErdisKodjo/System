/*
 * package_manager/permission_manager.cpp — Runtime permission manager.
 *
 * Android's permission model has two halves: install-time permissions
 * (granted at install, listed in <uses-permission>) and runtime
 * permissions (must be prompted for at first use; grouped into
 * permission groups like CAMERA, LOCATION, MICROPHONE). This module
 * implements both: a (package, permission) → granted table, plus the
 * helpers Grant/Revoke/Check that PMS and the framework call into.
 *
 * Dangerous permissions start in the "denied" state and must be granted
 * explicitly; normal permissions are auto-granted at install time. The
 * distinction is encoded in a small static table of well-known dangerous
 * permission names.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>

/* Subset of dangerous (runtime) permission names. Anything not in this
 * set is treated as a normal/install-time permission and is auto-granted. */
static const char *kDangerous[] = {
    "android.permission.CAMERA",
    "android.permission.RECORD_AUDIO",
    "android.permission.ACCESS_FINE_LOCATION",
    "android.permission.ACCESS_COARSE_LOCATION",
    "android.permission.READ_CONTACTS",
    "android.permission.WRITE_CONTACTS",
    "android.permission.READ_EXTERNAL_STORAGE",
    "android.permission.WRITE_EXTERNAL_STORAGE",
    "android.permission.READ_CALENDAR",
    "android.permission.WRITE_CALENDAR",
    "android.permission.READ_SMS",
    "android.permission.SEND_SMS",
    "android.permission.READ_PHONE_STATE",
    "android.permission.CALL_PHONE",
    "android.permission.BODY_SENSORS",
    nullptr,
};

static bool is_dangerous(const char *perm) {
    for (size_t i = 0; kDangerous[i]; i++) {
        if (std::strcmp(perm, kDangerous[i]) == 0) return true;
    }
    return false;
}

struct PermKey {
    std::string pkg;
    std::string perm;
    bool operator==(const PermKey &o) const {
        return pkg == o.pkg && perm == o.perm;
    }
};

struct PermKeyHash {
    size_t operator()(const PermKey &k) const noexcept {
        return std::hash<std::string>()(k.pkg) ^
               (std::hash<std::string>()(k.perm) << 1);
    }
};

class PermissionManager {
public:
    /* Grant a permission to a package. Returns OK on success, ALREADY_EXISTS
     * if already granted. Dangerous permissions require an explicit grant
     * call; install-time permissions are auto-granted on first check. */
    status_t Grant(const char *pkg, const char *perm) {
        if (!pkg || !perm) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        PermKey k{pkg, perm};
        auto it = table_.find(k);
        if (it != table_.end() && it->second) return ALREADY_EXISTS;
        table_[k] = true;
        return OK;
    }

    status_t Revoke(const char *pkg, const char *perm) {
        if (!pkg || !perm) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        PermKey k{pkg, perm};
        auto it = table_.find(k);
        if (it == table_.end() || !it->second) return NAME_NOT_FOUND;
        it->second = false;
        return OK;
    }

    /* Returns 1 if granted, 0 if not. Auto-grants install-time permissions. */
    int Check(const char *pkg, const char *perm) {
        if (!pkg || !perm) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        PermKey k{pkg, perm};
        auto it = table_.find(k);
        if (it != table_.end()) return it->second ? 1 : 0;
        if (!is_dangerous(perm)) {
            table_[k] = true;
            return 1;
        }
        return 0;
    }

    /* List all permissions granted to `pkg`. */
    size_t ListForPackage(const char *pkg, const char **out, size_t max) {
        if (!pkg) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = 0;
        for (auto &kv : table_) {
            if (kv.first.pkg == pkg && kv.second && n < max) {
                out[n++] = kv.first.perm.c_str();
            }
        }
        return n;
    }

    /* Is the permission dangerous (requires runtime prompt)? */
    int IsDangerous(const char *perm) {
        return is_dangerous(perm) ? 1 : 0;
    }

private:
    std::mutex mu_;
    std::unordered_map<PermKey, bool, PermKeyHash> table_;
};

static PermissionManager *g_pm = nullptr;
static PermissionManager *pm() {
    if (!g_pm) g_pm = new PermissionManager();
    return g_pm;
}

extern "C" {

int PermissionGrant(const char *pkg, const char *perm) {
    return (int)pm()->Grant(pkg, perm);
}
int PermissionRevoke(const char *pkg, const char *perm) {
    return (int)pm()->Revoke(pkg, perm);
}
int PermissionCheck(const char *pkg, const char *perm) {
    return pm()->Check(pkg, perm);
}
size_t PermissionListForPackage(const char *pkg, const char **out, size_t max) {
    return pm()->ListForPackage(pkg, out, max);
}
int PermissionIsDangerous(const char *perm) {
    return pm()->IsDangerous(perm);
}

} /* extern "C" */
