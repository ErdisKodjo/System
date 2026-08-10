/*
 * surfaceflinger/surface_flinger.cpp — Main composer service.
 *
 * SurfaceFlinger is Android's display compositor. It owns the frame buffer
 * (or its modern equivalent — a compositor target supplied by the HWComposer
 * HAL), walks the layer list every vsync, composites visible layers in
 * z-order, applies per-layer transforms (crop, alpha, position), and posts
 * the result to the display. It also drives the frame timeline (vsync,
 * frame deadline, present time).
 *
 * In the sandbox we run a real vsync thread (a 60 Hz timer) and a real
 * composition loop (driven by SurfaceFlingerLoop()); the actual pixel
 * writes are no-ops since there is no real display, but every other piece
 * of state (layers, layers dirty bit, frame counter, HWC prepare/set) is
 * maintained for real.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <condition_variable>

extern "C" {
    int  HwcInit(int (*vsync_cb)(void *), void *ctx);
    void HwcShutdown(void);
    int  HwcPrepare(int n_layers, const uint32_t *layer_ids);
    int  HwcSet(int n_layers, const uint32_t *layer_ids);
    int  HwcGetDisplayWidth(void);
    int  HwcGetDisplayHeight(void);
}

class Layer;
class BufferQueue;

/* Per-layer state (defined in layer.cpp; we only need a forward decl). */
struct LayerState {
    uint32_t id;
    int      z_order;
    int      x, y, w, h;
    float    alpha;
    bool     visible;
    bool     dirty;
};

class SurfaceFlinger {
public:
    SurfaceFlinger()
        : running_(false), vsync_count_(0), frame_count_(0),
          width_(0), height_(0), hwc_inited_(false) {}

    status_t Init() {
        if (running_.exchange(true)) return ALREADY_EXISTS;
        width_  = HwcGetDisplayWidth();
        height_ = HwcGetDisplayHeight();
        if (width_  <= 0) width_  = 1080;
        if (height_ <= 0) height_ = 1920;
        if (HwcInit(&SurfaceFlinger::VsyncCallback, this) == 0) hwc_inited_ = true;
        vsync_thread_ = std::thread([this] { VsyncLoop(); });
        return OK;
    }

    void Shutdown() {
        if (!running_.exchange(false)) return;
        cv_.notify_all();
        if (vsync_thread_.joinable()) vsync_thread_.join();
        if (hwc_inited_) { HwcShutdown(); hwc_inited_ = false; }
    }

    /* Main composition loop — call from the dedicated SF thread. Returns
     * when Shutdown() is invoked. */
    void Loop() {
        while (running_.load()) {
            std::unique_lock<std::mutex> lk(mu_);
            if (cv_.wait_for(lk, std::chrono::milliseconds(16),
                             [this] { return !running_.load() || dirty_.load(); })) {
                if (!running_.load()) break;
                CompositeLocked();
                dirty_ = false;
            }
        }
    }

    uint32_t AddLayer(const LayerState &s) {
        std::lock_guard<std::mutex> lk(mu_);
        LayerState ns = s;
        ns.id = next_id_++;
        layers_.push_back(ns);
        dirty_ = true;
        cv_.notify_one();
        return ns.id;
    }

    status_t RemoveLayer(uint32_t id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = layers_.begin(); it != layers_.end(); ++it) {
            if (it->id == id) { layers_.erase(it); dirty_ = true; return OK; }
        }
        return NAME_NOT_FOUND;
    }

    status_t UpdateLayer(uint32_t id, const LayerState &s) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &l : layers_) {
            if (l.id == id) {
                l.z_order = s.z_order; l.x = s.x; l.y = s.y;
                l.w = s.w; l.h = s.h; l.alpha = s.alpha;
                l.visible = s.visible; l.dirty = true;
                dirty_ = true;
                cv_.notify_one();
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    void Invalidate() { dirty_ = true; cv_.notify_one(); }

    uint64_t VsyncCount()  const { return vsync_count_.load(); }
    uint64_t FrameCount()  const { return frame_count_.load(); }
    int      Width()       const { return width_; }
    int      Height()      const { return height_; }

private:
    void VsyncLoop() {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::microseconds(16667));
            vsync_count_++;
            dirty_ = true;
            cv_.notify_one();
        }
    }

    void CompositeLocked() {
        /* Sort by z-order (stable). */
        std::vector<LayerState> sorted = layers_;
        std::sort(sorted.begin(), sorted.end(),
                  [](const LayerState &a, const LayerState &b) {
                      return a.z_order < b.z_order;
                  });
        std::vector<uint32_t> ids;
        for (auto &l : sorted) if (l.visible) ids.push_back(l.id);
        HwcPrepare((int)ids.size(), ids.data());
        HwcSet((int)ids.size(), ids.data());
        frame_count_++;
    }

    static int VsyncCallback(void *ctx) {
        SurfaceFlinger *sf = static_cast<SurfaceFlinger *>(ctx);
        sf->vsync_count_++;
        sf->dirty_ = true;
        sf->cv_.notify_one();
        return 0;
    }

    std::atomic<bool>      running_;
    std::atomic<uint64_t>  vsync_count_;
    std::atomic<uint64_t>  frame_count_;
    std::atomic<bool>      dirty_{false};
    int                    width_, height_;
    bool                   hwc_inited_;
    std::thread            vsync_thread_;
    std::mutex             mu_;
    std::condition_variable cv_;
    std::vector<LayerState> layers_;
    uint32_t               next_id_{1};
};

static SurfaceFlinger *g_sf = nullptr;
static SurfaceFlinger *sf() {
    if (!g_sf) g_sf = new SurfaceFlinger();
    return g_sf;
}

extern "C" {

status_t SurfaceFlingerInit(void)  { return sf()->Init(); }
void     SurfaceFlingerShutdown()  { sf()->Shutdown(); }
void     SurfaceFlingerLoop(void)  { sf()->Loop(); }
uint32_t SurfaceFlingerAddLayer(int z, int x, int y, int w, int h,
                                float alpha, bool visible) {
    LayerState s{};
    s.z_order = z; s.x = x; s.y = y; s.w = w; s.h = h;
    s.alpha = alpha; s.visible = visible; s.dirty = true;
    return sf()->AddLayer(s);
}
status_t SurfaceFlingerRemoveLayer(uint32_t id) { return sf()->RemoveLayer(id); }
status_t SurfaceFlingerUpdateLayer(uint32_t id, int z, int x, int y,
                                   int w, int h, float alpha, bool vis) {
    LayerState s{};
    s.z_order = z; s.x = x; s.y = y; s.w = w; s.h = h;
    s.alpha = alpha; s.visible = vis; s.dirty = true;
    return sf()->UpdateLayer(id, s);
}
uint64_t SurfaceFlingerVsyncCount() { return sf()->VsyncCount(); }
uint64_t SurfaceFlingerFrameCount() { return sf()->FrameCount(); }

} /* extern "C" */
