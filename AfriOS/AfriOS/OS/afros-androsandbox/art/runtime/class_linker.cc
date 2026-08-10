/*
 * art/runtime/class_linker.cc — Class resolution & class table.
 *
 * The ClassLinker is the part of ART responsible for turning .dex files
 * into in-memory Class objects, resolving references between classes, and
 * answering "give me the Class* for descriptor Lfoo/Bar;". This module
 * maintains a global class table (hash map from descriptor → ArtClass)
 * and exposes the two entry points the rest of the sandbox needs:
 *
 *   ClassLinkerDefineClass()  — register a class (with its DEX bytes).
 *   ClassLinkerLookupClass()  — fetch a previously-defined class.
 *
 * It also exposes ClassLinkerInit()/Shutdown() (used by ArtRuntime) and a
 * few helpers used by the JIT (ClassLinkerForEachMethod).
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <mutex>

/* Forward-declared in art_runtime.cc; defined here. */
struct ArtMethod {
    char name[64];
    char signature[64];
    uint32_t code_offset;
    uint32_t access_flags;
    void    *native_code; /* filled in by the JIT */
};

struct ArtClass {
    char     descriptor[128]; /* e.g. "Lcom/example/Foo;" */
    char     simple_name[64];
    uint32_t access_flags;
    uint32_t super_class_idx;
    ArtClass *super;
    std::vector<ArtMethod> methods;
    std::vector<ArtClass *> interfaces;
    size_t   object_size;
    bool     initialized;
};

static std::unordered_map<std::string, ArtClass *> g_table;
static std::mutex g_lock;
static bool g_inited = false;

extern "C" status_t ClassLinkerInit(void) {
    if (g_inited) return ALREADY_EXISTS;
    g_inited = true;
    /* Register a few well-known core classes so resolution succeeds. */
    static const char *kCore[] = {
        "Ljava/lang/Object;",
        "Ljava/lang/String;",
        "Ljava/lang/Class;",
        "Ljava/lang/Thread;",
        "Ljava/lang/Throwable;",
        "Ljava/lang/System;",
        "Ljava/lang/Runtime;",
        "Ljava/lang/Integer;",
        "Ljava/lang/StringBuilder;",
    };
    for (const char *d : kCore) {
        ArtClass *c = new ArtClass();
        c->descriptor[0] = 0;
        c->simple_name[0] = 0;
        c->access_flags = 0;
        c->super_class_idx = 0;
        c->super = nullptr;
        c->object_size = 16;
        c->initialized = true;
        std::strncpy(c->descriptor, d, sizeof(c->descriptor) - 1);
        g_table[d] = c;
    }
    return OK;
}

extern "C" void ClassLinkerShutdown(void) {
    std::lock_guard<std::mutex> lk(g_lock);
    for (auto &kv : g_table) delete kv.second;
    g_table.clear();
    g_inited = false;
}

static ArtClass *make_class(const char *descriptor) {
    ArtClass *c = new ArtClass();
    c->descriptor[0] = 0;
    c->simple_name[0] = 0;
    c->access_flags = 0;
    c->super_class_idx = 0;
    c->super = nullptr;
    c->object_size = 16;
    c->initialized = false;
    std::strncpy(c->descriptor, descriptor, sizeof(c->descriptor) - 1);
    /* Derive a simple name from the descriptor: Lcom/example/Foo; -> Foo. */
    const char *slash = std::strrchr(descriptor, '/');
    const char *start = slash ? slash + 1 : descriptor + 1;
    std::strncpy(c->simple_name, start, sizeof(c->simple_name) - 1);
    char *semi = std::strchr(c->simple_name, ';');
    if (semi) *semi = 0;
    c->object_size = 16;
    c->initialized = false;
    return c;
}

extern "C" status_t ClassLinkerDefineClass(const char *descriptor,
                                           const void * /*dex*/,
                                           size_t /*len*/) {
    if (!descriptor) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_table.find(descriptor);
    if (it != g_table.end()) return ALREADY_EXISTS;
    ArtClass *c = make_class(descriptor);
    g_table[descriptor] = c;
    return OK;
}

extern "C" status_t ClassLinkerLookupClass(const char *descriptor, void **out) {
    if (!descriptor || !out) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_table.find(descriptor);
    if (it == g_table.end()) return NAME_NOT_FOUND;
    *out = it->second;
    return OK;
}

/* Mark a class as initialized (runs <clinit> in real ART). */
extern "C" status_t ClassLinkerEnsureInitialized(const char *descriptor) {
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_table.find(descriptor);
    if (it == g_table.end()) return NAME_NOT_FOUND;
    it->second->initialized = true;
    return OK;
}

/* Register a method on a class; used by the DEX loader / JIT. */
extern "C" status_t ClassLinkerAddMethod(const char *descriptor,
                                         const char *name,
                                         const char *sig,
                                         uint32_t code_off,
                                         uint32_t access_flags) {
    std::lock_guard<std::mutex> lk(g_lock);
    auto it = g_table.find(descriptor);
    if (it == g_table.end()) return NAME_NOT_FOUND;
    ArtMethod m;
    m.name[0] = 0;
    m.signature[0] = 0;
    m.code_offset = code_off;
    m.access_flags = access_flags;
    m.native_code = nullptr;
    std::strncpy(m.name, name ? name : "", sizeof(m.name) - 1);
    std::strncpy(m.signature, sig ? sig : "", sizeof(m.signature) - 1);
    it->second->methods.push_back(m);
    return OK;
}

/* Enumerate all methods of all classes; callback returns non-zero to stop. */
extern "C" void ClassLinkerForEachMethod(int (*cb)(const char *cls,
                                                   const char *m,
                                                   const char *sig,
                                                   void *ctx),
                                         void *ctx) {
    std::lock_guard<std::mutex> lk(g_lock);
    for (auto &kv : g_table) {
        for (auto &m : kv.second->methods) {
            if (cb(kv.first.c_str(), m.name, m.signature, ctx)) return;
        }
    }
}

extern "C" size_t ClassLinkerCount(void) {
    std::lock_guard<std::mutex> lk(g_lock);
    return g_table.size();
}
