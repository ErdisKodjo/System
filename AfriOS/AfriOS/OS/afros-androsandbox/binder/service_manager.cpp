/*
 * binder/service_manager.cpp — IServiceManager implementation.
 *
 * The Service Manager is the well-known binder context manager (handle 0)
 * that lets services publish themselves under a string name and lets
 * clients look them up by name. This module maintains an in-memory
 * service table mapping name → binder_handle_t (obtained from
 * ReferenceTrackerRegister), seeds it with the default Android framework
 * services (activity, package, window, etc. — registered as placeholders
 * so client lookups succeed even before the real services come up), and
 * exposes addService/getService/checkService/listServices.
 *
 * The implementation mirrors android.os.ServiceManager / IServiceManager
 * but lives in-process — there is no real /dev/binder in the sandbox.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <mutex>

extern "C" {
    binder_handle_t RefTrackerRegister(void *cookie);
    int   RefTrackerAcquireStrong(binder_handle_t h);
    int   RefTrackerReleaseStrong(binder_handle_t h);
}

struct ServiceEntry {
    String8 name;
    binder_handle_t handle;
    void *cookie;
};

class ServiceManager {
public:
    ServiceManager() {
        /* Seed the default Android framework services. Each gets a ref-tracker
         * node with a sentinel cookie so that getService() succeeds before the
         * real service implementation registers. */
        static const char *kDefault[] = {
            "activity", "package", "window", "power",
            "notification", "location", "audio", "sensor",
            "camera", "telephony.registry", "media", "input",
            "connectivity", "wifi", "bluetooth",
        };
        for (const char *n : kDefault) {
            addService(n, nullptr, /*weak=*/true);
        }
    }

    status_t addService(const char *name, void *cookie, bool weak = false) {
        if (!name) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : services_) {
            if (e.name == name) {
                e.cookie = cookie;
                if (cookie && e.handle == 0) {
                    e.handle = RefTrackerRegister(cookie);
                }
                return ALREADY_EXISTS;
            }
        }
        ServiceEntry e;
        e.name = String8(name);
        e.cookie = cookie;
        e.handle = cookie ? RefTrackerRegister(cookie) : 0;
        services_.push_back(e);
        return OK;
    }

    binder_handle_t getService(const char *name) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : services_) {
            if (e.name == name) {
                if (e.handle) RefTrackerAcquireStrong(e.handle);
                return e.handle;
            }
        }
        return 0; /* NAME_NOT_FOUND */
    }

    binder_handle_t checkService(const char *name) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : services_) {
            if (e.name == name) return e.handle; /* does not acquire */
        }
        return 0;
    }

    /* Enumerates services whose name starts with `prefix` (pass "" for all)
     * into the caller's array; returns the number filled. */
    size_t listServices(const char *prefix, String8 *out, size_t max) {
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = 0;
        for (auto &e : services_) {
            if (n >= max) break;
            if (!prefix || prefix[0] == 0 ||
                strncmp(e.name.c_str(), prefix, strlen(prefix)) == 0) {
                out[n++] = e.name;
            }
        }
        return n;
    }

    size_t count() {
        std::lock_guard<std::mutex> lk(mu_);
        return services_.size();
    }

    void release(binder_handle_t h) {
        if (h) RefTrackerReleaseStrong(h);
    }

private:
    std::mutex mu_;
    std::vector<ServiceEntry> services_;
};

static ServiceManager *g_sm = nullptr;
static ServiceManager *sm() {
    if (!g_sm) g_sm = new ServiceManager();
    return g_sm;
}

extern "C" {

status_t       ServiceManagerAdd(const char *name, void *cookie) {
    return sm()->addService(name, cookie);
}
binder_handle_t ServiceManagerGet(const char *name) {
    return sm()->getService(name);
}
binder_handle_t ServiceManagerCheck(const char *name) {
    return sm()->checkService(name);
}
size_t ServiceManagerList(const char *prefix, const char **out, size_t max) {
    /* For C callers, copy the names into a static array. */
    static thread_local std::vector<String8> buf;
    buf.resize(max);
    size_t n = sm()->listServices(prefix, buf.data(), max);
    for (size_t i = 0; i < n && i < max; i++) out[i] = buf[i].c_str();
    return n;
}
size_t ServiceManagerCount() { return sm()->count(); }
void   ServiceManagerRelease(binder_handle_t h) { sm()->release(h); }

} /* extern "C" */
