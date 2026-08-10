/*
 * art/runtime/art_runtime.cc — ART runtime singleton.
 *
 * ART (Android Runtime) replaces the legacy Dalvik VM. A single ArtRuntime
 * instance per process owns the heap, GC threads, JIT compiler, thread list,
 * and the class linker. This file implements the lifecycle:
 *
 *   ArtRuntimeStart()    — boot the runtime (heap, JIT, GC, signal handlers).
 *   ArtRuntimeShutdown() — stop GC threads, flush JIT cache, free heap.
 *   ArtRuntimeGetInstance() — return the singleton (NULL before Start()).
 *
 * The sandbox doesn't run actual bytecode; this module still creates all
 * the right sub-objects and exposes the standard lifecycle hooks so that
 * the rest of the sandbox (dalvikvm, dex2oat, framework services) can be
 * written against the real API.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

extern "C" {
    status_t ClassLinkerInit(void);
    void    ClassLinkerShutdown(void);
    status_t JitInit(void);
    void    JitShutdown(void);
}

namespace afros_art {

/* Heap configuration — the default matches a low-end Android device. */
struct HeapConfig {
    size_t initial_size;       /* bytes */
    size_t max_size;           /* bytes */
    size_t growth_limit;       /* bytes */
    bool   low_memory_mode;
};

class Heap {
public:
    explicit Heap(const HeapConfig &c)
        : config_(c), allocated_(0), peak_(0), gc_count_(0) {
        base_ = std::malloc(c.initial_size);
    }
    ~Heap() { if (base_) std::free(base_); }

    void *Alloc(size_t n) {
        std::lock_guard<std::mutex> lk(mu_);
        if (allocated_ + n > config_.max_size) return nullptr;
        void *p = std::malloc(n);
        if (p) {
            allocated_ += n;
            if (allocated_ > peak_) peak_ = allocated_;
        }
        return p;
    }
    void Free(void *p, size_t n) {
        if (!p) return;
        std::lock_guard<std::mutex> lk(mu_);
        std::free(p);
        if (allocated_ >= n) allocated_ -= n;
    }
    size_t GetUsed()      { std::lock_guard<std::mutex> lk(mu_); return allocated_; }
    size_t GetFree()      { std::lock_guard<std::mutex> lk(mu_); return config_.max_size - allocated_; }
    size_t GetMax()       { return config_.max_size; }
    size_t GetGcCount()   { return gc_count_; }

    /* Run a (no-op) garbage collection cycle; bumps gc_count_. */
    void CollectGarbage() {
        std::lock_guard<std::mutex> lk(mu_);
        gc_count_++;
    }

private:
    HeapConfig config_;
    void *base_;
    size_t allocated_;
    size_t peak_;
    size_t gc_count_;
    std::mutex mu_;
};

class ArtRuntime {
public:
    ArtRuntime() : started_(false), gc_thread_running_(false), gc_interval_ms_(1000) {}
    ~ArtRuntime() { Shutdown(); }

    status_t Start(int /*argc*/, char ** /*argv*/) {
        if (started_.exchange(true)) return ALREADY_EXISTS;
        HeapConfig hc;
        hc.initial_size    = 4 * 1024 * 1024;
        hc.max_size        = 64 * 1024 * 1024;
        hc.growth_limit    = 128 * 1024 * 1024;
        hc.low_memory_mode = false;
        heap_.reset(new Heap(hc));
        ClassLinkerInit();
        JitInit();
        StartGcThread();
        return OK;
    }

    status_t Shutdown() {
        if (!started_.exchange(false)) return OK;
        StopGcThread();
        JitShutdown();
        ClassLinkerShutdown();
        if (heap_) heap_->CollectGarbage();
        heap_.reset();
        return OK;
    }

    Heap *GetHeap() { return heap_.get(); }
    bool  IsStarted() const { return started_.load(); }

    /* Allocate an object from the managed heap. */
    void *AllocObject(size_t n) {
        return heap_ ? heap_->Alloc(n) : nullptr;
    }

    /* Forcibly run a GC cycle (used by System.gc() and tests). */
    void RequestGc() {
        if (heap_) heap_->CollectGarbage();
    }

private:
    void StartGcThread() {
        gc_thread_running_ = true;
        gc_thread_ = std::thread([this] {
            while (gc_thread_running_) {
                std::unique_lock<std::mutex> lk(gc_mu_);
                gc_cv_.wait_for(lk, std::chrono::milliseconds(gc_interval_ms_),
                                [this] { return !gc_thread_running_; });
                if (!gc_thread_running_) break;
                if (heap_) heap_->CollectGarbage();
            }
        });
    }
    void StopGcThread() {
        {
            std::lock_guard<std::mutex> lk(gc_mu_);
            gc_thread_running_ = false;
            gc_cv_.notify_all();
        }
        if (gc_thread_.joinable()) gc_thread_.join();
    }

    std::atomic<bool> started_;
    std::unique_ptr<Heap> heap_;
    std::thread       gc_thread_;
    std::atomic<bool> gc_thread_running_;
    int               gc_interval_ms_;
    std::mutex        gc_mu_;
    std::condition_variable gc_cv_;
};

} /* namespace afros_art */

static afros_art::ArtRuntime *g_runtime = nullptr;

extern "C" {

status_t ArtRuntimeStart(int argc, char **argv) {
    if (!g_runtime) g_runtime = new afros_art::ArtRuntime();
    return g_runtime->Start(argc, argv);
}

status_t ArtRuntimeShutdown(void) {
    if (!g_runtime) return NO_INIT;
    return g_runtime->Shutdown();
}

void *ArtRuntimeGetInstance(void) {
    return static_cast<void *>(g_runtime);
}

/* Convenience entry: trigger a GC cycle. */
void ArtRuntimeRequestGc(void) {
    if (g_runtime) g_runtime->RequestGc();
}

/* Convenience entry: allocate an object on the managed heap. */
void *ArtRuntimeAllocObject(size_t n) {
    return g_runtime ? g_runtime->AllocObject(n) : nullptr;
}

} /* extern "C" */
