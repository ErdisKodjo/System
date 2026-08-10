/*
 * binder/reference_tracker.cpp — Strong/weak reference counting for IBinder.
 *
 * Mirrors the kernel-side ref-only model used by the real binder driver: every
 * binder reference held by a process is either strong (BC_ACQUIRE/BC_RELEASE)
 * or weak (BC_INCREFS/BC_DECREFS). When the last strong ref on a node goes
 * away, death recipients linked to the node are notified. This module keeps a
 * global table of nodes (server-side binder objects) and references (client-
 * side handles) so that the framework's sp<IBinder>/wp<IBinder> smart
 * pointers can be backed by real ref counting semantics.
 *
 * The implementation is intentionally simple — a fixed-size open-addressed
 * hash table — since the sandbox only needs to model thousands of refs, not
 * millions.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <vector>
#include <mutex>

#define REF_TABLE_SIZE   1024
#define DEATH_LIST_MAX   16

struct BinderNode;

struct DeathRecipient {
    void (*callback)(void *cookie, binder_handle_t handle);
    void *cookie;
};

struct BinderRef {
    bool      in_use;
    bool      is_weak;
    int       strong_count;
    int       weak_count;
    BinderNode *node;
    binder_handle_t handle;
};

struct BinderNode {
    bool      in_use;
    void     *cookie;
    int       local_strong;
    int       local_weak;
    std::vector<DeathRecipient> deaths;
};

static BinderNode g_nodes[REF_TABLE_SIZE];
static BinderRef  g_refs[REF_TABLE_SIZE];
static std::mutex g_ref_lock;
static binder_handle_t g_next_handle = 1;

static BinderNode *node_alloc(void *cookie) {
    for (int i = 0; i < REF_TABLE_SIZE; i++) {
        if (!g_nodes[i].in_use) {
            g_nodes[i].in_use = true;
            g_nodes[i].cookie = cookie;
            g_nodes[i].local_strong = 0;
            g_nodes[i].local_weak = 0;
            g_nodes[i].deaths.clear();
            return &g_nodes[i];
        }
    }
    return nullptr;
}

static BinderRef *ref_alloc(BinderNode *n) {
    for (int i = 0; i < REF_TABLE_SIZE; i++) {
        if (!g_refs[i].in_use) {
            g_refs[i].in_use = true;
            g_refs[i].is_weak = false;
            g_refs[i].strong_count = 0;
            g_refs[i].weak_count = 0;
            g_refs[i].node = n;
            g_refs[i].handle = g_next_handle++;
            n->local_strong++;
            return &g_refs[i];
        }
    }
    return nullptr;
}

static BinderRef *ref_find(binder_handle_t h) {
    for (int i = 0; i < REF_TABLE_SIZE; i++) {
        if (g_refs[i].in_use && g_refs[i].handle == h) return &g_refs[i];
    }
    return nullptr;
}

static void fire_deaths(BinderNode *n) {
    for (auto &d : n->deaths) {
        if (d.callback) d.callback(d.cookie, 0);
    }
    n->deaths.clear();
}

class ReferenceTracker {
public:
    /* Acquire a strong reference on a handle; returns the new count. */
    int AcquireStrong(binder_handle_t h) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r) return -1;
        r->strong_count++;
        if (r->node) r->node->local_strong++;
        return r->strong_count;
    }
    /* Release a strong reference; fires death recipients if last. */
    int ReleaseStrong(binder_handle_t h) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r || r->strong_count <= 0) return -1;
        r->strong_count--;
        if (r->node) {
            r->node->local_strong--;
            if (r->node->local_strong == 0) fire_deaths(r->node);
        }
        if (r->strong_count == 0 && r->weak_count == 0) r->in_use = false;
        return r->strong_count;
    }
    /* Acquire/release a weak reference. */
    int AcquireWeak(binder_handle_t h) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r) return -1;
        r->weak_count++;
        if (r->node) r->node->local_weak++;
        return r->weak_count;
    }
    int ReleaseWeak(binder_handle_t h) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r || r->weak_count <= 0) return -1;
        r->weak_count--;
        if (r->node) r->node->local_weak--;
        if (r->strong_count == 0 && r->weak_count == 0) r->in_use = false;
        return r->weak_count;
    }
    /* Register a new server-side node and return its first handle. */
    binder_handle_t RegisterNode(void *cookie) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderNode *n = node_alloc(cookie);
        if (!n) return 0;
        BinderRef *r = ref_alloc(n);
        return r ? r->handle : 0;
    }
    /* Register a death recipient on a handle. */
    status_t LinkToDeath(binder_handle_t h, void (*cb)(void *, binder_handle_t),
                         void *cookie) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r || !r->node) return NAME_NOT_FOUND;
        if (r->node->deaths.size() >= DEATH_LIST_MAX) return NO_MEMORY;
        DeathRecipient d;
        d.callback = cb;
        d.cookie = cookie;
        r->node->deaths.push_back(d);
        return OK;
    }
    status_t UnlinkToDeath(binder_handle_t h, void *cookie) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        BinderRef *r = ref_find(h);
        if (!r || !r->node) return NAME_NOT_FOUND;
        for (auto it = r->node->deaths.begin(); it != r->node->deaths.end(); ++it) {
            if (it->cookie == cookie) { r->node->deaths.erase(it); return OK; }
        }
        return NAME_NOT_FOUND;
    }
    /* Drop a node entirely (server-side exit); fires all death recipients. */
    void KillNode(void *cookie) {
        std::lock_guard<std::mutex> lk(g_ref_lock);
        for (int i = 0; i < REF_TABLE_SIZE; i++) {
            if (g_nodes[i].in_use && g_nodes[i].cookie == cookie) {
                fire_deaths(&g_nodes[i]);
                g_nodes[i].in_use = false;
            }
        }
    }
};

static ReferenceTracker *g_tracker = nullptr;
static ReferenceTracker *tracker() {
    if (!g_tracker) g_tracker = new ReferenceTracker();
    return g_tracker;
}

extern "C" {

binder_handle_t RefTrackerRegister(void *cookie) {
    return tracker()->RegisterNode(cookie);
}
int   RefTrackerAcquireStrong(binder_handle_t h) { return tracker()->AcquireStrong(h); }
int   RefTrackerReleaseStrong(binder_handle_t h) { return tracker()->ReleaseStrong(h); }
int   RefTrackerAcquireWeak(binder_handle_t h)   { return tracker()->AcquireWeak(h); }
int   RefTrackerReleaseWeak(binder_handle_t h)   { return tracker()->ReleaseWeak(h); }
status_t RefTrackerLinkToDeath(binder_handle_t h,
                               void (*cb)(void *, binder_handle_t),
                               void *cookie) {
    return tracker()->LinkToDeath(h, cb, cookie);
}
status_t RefTrackerUnlinkToDeath(binder_handle_t h, void *cookie) {
    return tracker()->UnlinkToDeath(h, cookie);
}
void  RefTrackerKillNode(void *cookie) { tracker()->KillNode(cookie); }

} /* extern "C" */
