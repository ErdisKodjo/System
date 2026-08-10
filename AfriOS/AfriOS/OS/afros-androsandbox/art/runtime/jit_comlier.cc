/*
 * art/runtime/jit_comlier.cc — JIT compiler.
 *
 * ART's JIT compiles frequently-executed (hot) methods to native code at
 * runtime, caching the result in a code cache. The compilation trigger is
 * a method's invocation count crossing a threshold; if a method is hot
 * while it is already on the stack, on-stack replacement (OSR) may swap
 * the interpreter frame for a JIT'd frame.
 *
 * This module provides a simple method-keyed cache: JitCompileMethod()
 * "compiles" a method by allocating an executable page and recording a
 * stub entry point; JitInvokeMethod() looks up and "invokes" it. No actual
 * machine code is emitted — the entry point is a small x86_64/aarch64
 * stub that simply returns 0.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

extern "C" {
    status_t ClassLinkerInit(void);
}

/* JIT configuration. */
#define JIT_METHOD_THRESHOLD   10   /* invocations before compile */
#define JIT_CACHE_MAX          256  /* max cached methods */
#define JIT_CODE_PAGE_SIZE     4096

struct JitEntry {
    void    *code;          /* executable page */
    uint32_t method_id;     /* unique key */
    uint32_t invoke_count;  /* times invoked since compile */
    uint32_t hit_count;     /* times the cache hit */
    bool     osr_eligible;
};

class JitCache {
public:
    JitCache() : next_id_(1) {}
    ~JitCache() { flush(); }

    /* Lookup by method id; returns nullptr on miss. */
    JitEntry *Lookup(uint32_t method_id) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = entries_.find(method_id);
        if (it == entries_.end()) return nullptr;
        it->second.hit_count++;
        return &it->second;
    }

    /* Compile a method: allocate a code page, install a stub. */
    JitEntry *Compile(uint32_t method_id) {
        std::lock_guard<std::mutex> lk(mu_);
        if (entries_.size() >= JIT_CACHE_MAX) {
            /* Naive eviction: drop the entry with the lowest hit_count. */
            auto victim = entries_.begin();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (it->second.hit_count < victim->second.hit_count) victim = it;
            }
            munmap(victim->second.code, JIT_CODE_PAGE_SIZE);
            entries_.erase(victim);
        }
        void *page = mmap(nullptr, JIT_CODE_PAGE_SIZE,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED) return nullptr;
        /* Install a tiny "return 0" stub.
         *   x86_64: 31 c0             xor eax, eax
         *           c3                ret
         *   aarch64: d2800000   movz x0, #0
         *           d65f03c0   ret
         * We write both sequences; only one will be executed by the host. */
        unsigned char x86[] = { 0x31, 0xc0, 0xc3 };
        unsigned char arm[] = { 0x00, 0x00, 0x80, 0xd2,
                                0xc0, 0x03, 0x5f, 0xd6 };
        std::memcpy(page, x86, sizeof(x86));
        std::memcpy((unsigned char *)page + 16, arm, sizeof(arm));

        JitEntry e;
        e.code = page;
        e.method_id = method_id;
        e.invoke_count = 0;
        e.hit_count = 0;
        e.osr_eligible = false;
        auto r = entries_.emplace(method_id, e);
        return &r.first->second;
    }

    /* Account an invocation; compiles if threshold reached. */
    JitEntry *OnInvoke(uint32_t method_id) {
        JitEntry *e = Lookup(method_id);
        if (e) { e->invoke_count++; return e; }
        /* Not yet compiled — count toward threshold via a side table. */
        uint32_t count;
        {
            std::lock_guard<std::mutex> lk(mu_);
            uint32_t &c = pending_[method_id];
            c++;
            count = c;
        }
        if (count < JIT_METHOD_THRESHOLD) return nullptr;
        {
            std::lock_guard<std::mutex> lk(mu_);
            pending_.erase(method_id);
        }
        return Compile(method_id);
    }

    /* OSR: request compilation of a method that is currently on the stack. */
    status_t RequestOsr(uint32_t method_id) {
        JitEntry *e = Lookup(method_id);
        if (!e) e = Compile(method_id);
        if (!e) return FAILED_TRANSACTION;
        e->osr_eligible = true;
        return OK;
    }

    /* Invalidate a single entry (e.g. after class redefinition). */
    void Invalidate(uint32_t method_id) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = entries_.find(method_id);
        if (it != entries_.end()) {
            munmap(it->second.code, JIT_CODE_PAGE_SIZE);
            entries_.erase(it);
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &kv : entries_) munmap(kv.second.code, JIT_CODE_PAGE_SIZE);
        entries_.clear();
        pending_.clear();
    }

    size_t size() { std::lock_guard<std::mutex> lk(mu_); return entries_.size(); }

private:
    std::mutex mu_;
    std::unordered_map<uint32_t, JitEntry> entries_;
    std::unordered_map<uint32_t, uint32_t> pending_;
    std::atomic<uint32_t> next_id_;
};

static JitCache *g_cache = nullptr;
static JitCache *cache() {
    if (!g_cache) g_cache = new JitCache();
    return g_cache;
}

extern "C" {

status_t JitInit(void) {
    (void)cache();
    return OK;
}
void JitShutdown(void) {
    if (g_cache) { g_cache->flush(); delete g_cache; g_cache = nullptr; }
}

/* Compile a method by id; returns OK and fills *out_code on success. */
status_t JitCompileMethod(uint32_t method_id, void **out_code) {
    JitEntry *e = cache()->Lookup(method_id);
    if (!e) e = cache()->Compile(method_id);
    if (!e) return NO_MEMORY;
    if (out_code) *out_code = e->code;
    return OK;
}

/* Invoke a (possibly JIT'd) method; returns 0 by the stub. */
status_t JitInvokeMethod(uint32_t method_id, /*out*/ int64_t *result) {
    JitEntry *e = cache()->OnInvoke(method_id);
    if (result) *result = 0;
    /* Sandbox: do not actually call into the stub; the stub returns 0. */
    (void)e;
    return OK;
}

status_t JitRequestOsr(uint32_t method_id) {
    return cache()->RequestOsr(method_id);
}

void JitInvalidate(uint32_t method_id) { cache()->Invalidate(method_id); }
size_t JitCacheSize(void)              { return cache()->size(); }

} /* extern "C" */
