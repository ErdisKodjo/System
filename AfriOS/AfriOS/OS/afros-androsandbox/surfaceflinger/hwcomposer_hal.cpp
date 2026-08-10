/*
 * surfaceflinger/hwcomposer_hal.cpp — Hardware Composer HAL stub.
 *
 * The Hardware Composer (HWC) HAL is the contract between SurfaceFlinger
 * and the display controller hardware. For each frame SurfaceFlinger
 * calls HWC prepare() (so the HWC can claim layers it can composite in
 * hardware) and then HWC set() (to actually display the frame). The HWC
 * also delivers vsync events to SurfaceFlinger via a registered callback.
 *
 * This is a stub: it accepts every call, returns success, and drives the
 * vsync callback from a 60 Hz timer thread so SurfaceFlinger can run its
 * frame loop. Real HWC implementations would talk to a DRM/KMS or
 * AfriOS-specific display driver.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstring>

struct HwcState {
    std::mutex mu;
    int  (*vsync_cb)(void *) = nullptr;
    void *vsync_ctx = nullptr;
    std::atomic<bool> running{false};
    std::thread vsync_thread;
    std::atomic<uint64_t> vsync_count{0};
    int display_width{1080};
    int display_height{1920};
};

static HwcState *g_hwc = nullptr;
static HwcState *hwc() {
    if (!g_hwc) g_hwc = new HwcState();
    return g_hwc;
}

static void vsync_loop(HwcState *s) {
    while (s->running.load()) {
        std::this_thread::sleep_for(std::chrono::microseconds(16667));
        s->vsync_count++;
        int (*cb)(void *) = nullptr;
        void *ctx = nullptr;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            cb  = s->vsync_cb;
            ctx = s->vsync_ctx;
        }
        if (cb) cb(ctx);
    }
}

extern "C" int HwcInit(int (*vsync_cb)(void *), void *ctx) {
    HwcState *s = hwc();
    if (s->running.exchange(true)) return ALREADY_EXISTS;
    {
        std::lock_guard<std::mutex> lk(s->mu);
        s->vsync_cb  = vsync_cb;
        s->vsync_ctx = ctx;
    }
    s->vsync_thread = std::thread(vsync_loop, s);
    return OK;
}

extern "C" void HwcShutdown() {
    HwcState *s = hwc();
    if (!s->running.exchange(false)) return;
    if (s->vsync_thread.joinable()) s->vsync_thread.join();
}

/* prepare(): let the HWC inspect the layers and decide which it will
 * composite. The stub always returns "DEVICE" for every layer (i.e.
 * SurfaceFlinger must composite them itself). */
extern "C" int HwcPrepare(int n_layers, const uint32_t * /*layer_ids*/) {
    (void)n_layers;
    return OK;
}

/* set(): display the frame. The stub returns success. */
extern "C" int HwcSet(int n_layers, const uint32_t * /*layer_ids*/) {
    (void)n_layers;
    return OK;
}

extern "C" uint64_t HwcVsyncCount() {
    return hwc()->vsync_count.load();
}

extern "C" int HwcGetDisplayWidth()  { return hwc()->display_width; }
extern "C" int HwcGetDisplayHeight() { return hwc()->display_height; }

extern "C" void HwcSetDisplaySize(int w, int h) {
    HwcState *s = hwc();
    std::lock_guard<std::mutex> lk(s->mu);
    if (w > 0)  s->display_width  = w;
    if (h > 0)  s->display_height = h;
}

/* Dump HWC state into a caller-provided buffer. */
extern "C" int HwcDump(char *out, size_t out_max) {
    if (!out || out_max == 0) return BAD_VALUE;
    HwcState *s = hwc();
    std::memset(out, 0, out_max);
    int n = std::snprintf(out, out_max,
                          "HwcState: vsync=%llu display=%dx%d running=%d",
                          (unsigned long long)s->vsync_count.load(),
                          s->display_width, s->display_height,
                          (int)s->running.load());
    return n > 0 ? OK : UNKNOWN_TRANSACTION;
}
