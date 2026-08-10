/*
 * surfaceflinger/layer.cpp — Layer class.
 *
 * A Layer is the SurfaceFlinger-side representation of a single surface
 * owned by a client. It carries:
 *   - the producer/consumer BufferQueue that holds pending graphic buffers,
 *   - the position, z-order, crop rectangle, and alpha that the client
 *     sets via ANativeWindow API,
 *   - the per-frame dirty bit that SurfaceFlinger uses to skip compositing
 *     unchanged layers.
 *
 * This module implements the Layer class plus a tiny LayerRegistry that
 * hands out stable layer ids. SurfaceFlinger calls into the registry to
 * create/destroy/look up layers.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <map>
#include <mutex>

extern "C" {
    /* BufferQueue functions (buffer_queue.cpp). */
    void *BufferQueueCreate(int max);
    void  BufferQueueDestroy(void *q);
    int   BufferQueueQueueBuffer(void *q, const void *buf, size_t len);
    int   BufferQueueDequeueBuffer(void *q, void *out, size_t out_max, size_t *out_len);
    int   BufferQueueFreeAll(void *q);
}

class Layer {
public:
    Layer(uint32_t id)
        : id_(id), z_(0), x_(0), y_(0), w_(0), h_(0),
          alpha_(1.0f), visible_(true), dirty_(true),
          crop_left_(0), crop_top_(0), crop_right_(0), crop_bottom_(0),
          queue_(nullptr) {
        queue_ = BufferQueueCreate(8);
    }
    ~Layer() {
        if (queue_) BufferQueueDestroy(queue_);
    }

    uint32_t id() const { return id_; }

    /* Set geometry; marks the layer dirty. */
    void SetPosition(int x, int y) { x_ = x; y_ = y; dirty_ = true; }
    void SetSize(int w, int h)     { w_ = w; h_ = h; dirty_ = true; }
    void SetZ(int z)               { z_ = z; dirty_ = true; }
    void SetAlpha(float a)         { alpha_ = a; dirty_ = true; }
    void SetVisible(bool v)        { visible_ = v; dirty_ = true; }
    void SetCrop(int l, int t, int r, int b) {
        crop_left_ = l; crop_top_ = t;
        crop_right_ = r; crop_bottom_ = b;
        dirty_ = true;
    }

    int   X() const      { return x_; }
    int   Y() const      { return y_; }
    int   W() const      { return w_; }
    int   H() const      { return h_; }
    int   Z() const      { return z_; }
    float Alpha() const  { return alpha_; }
    bool  Visible() const { return visible_; }
    bool  Dirty() const  { return dirty_; }
    void  ClearDirty()   { dirty_ = false; }

    /* Producer side — client writes a frame. */
    int QueueBuffer(const void *buf, size_t len) {
        dirty_ = true;
        return BufferQueueQueueBuffer(queue_, buf, len);
    }
    /* Consumer side — SurfaceFlinger reads the most recent frame. */
    int DequeueBuffer(void *out, size_t out_max, size_t *out_len) {
        return BufferQueueDequeueBuffer(queue_, out, out_max, out_len);
    }

    void *queue() const { return queue_; }

private:
    uint32_t id_;
    int      z_, x_, y_, w_, h_;
    float    alpha_;
    bool     visible_;
    bool     dirty_;
    int      crop_left_, crop_top_, crop_right_, crop_bottom_;
    void    *queue_;
};

class LayerRegistry {
public:
    uint32_t Create() {
        std::lock_guard<std::mutex> lk(mu_);
        uint32_t id = next_id_++;
        Layer *l = new Layer(id);
        layers_[id] = l;
        return id;
    }
    Layer *Find(uint32_t id) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = layers_.find(id);
        return it != layers_.end() ? it->second : nullptr;
    }
    void Destroy(uint32_t id) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = layers_.find(id);
        if (it != layers_.end()) { delete it->second; layers_.erase(it); }
    }
    std::vector<uint32_t> List() {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<uint32_t> out;
        out.reserve(layers_.size());
        for (auto &kv : layers_) out.push_back(kv.first);
        return out;
    }
    void DestroyAll() {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &kv : layers_) delete kv.second;
        layers_.clear();
    }
private:
    std::mutex mu_;
    std::map<uint32_t, Layer *> layers_;
    uint32_t next_id_{1};
};

static LayerRegistry *g_reg = nullptr;
static LayerRegistry *reg() {
    if (!g_reg) g_reg = new LayerRegistry();
    return g_reg;
}

extern "C" {

uint32_t LayerCreate()                       { return reg()->Create(); }
void     LayerDestroy(uint32_t id)           { reg()->Destroy(id); }
void     LayerDestroyAll()                   { reg()->DestroyAll(); }
Layer   *LayerFind(uint32_t id)              { return reg()->Find(id); }

void     LayerSetPosition(uint32_t id, int x, int y) {
    if (Layer *l = reg()->Find(id)) l->SetPosition(x, y);
}
void     LayerSetSize(uint32_t id, int w, int h) {
    if (Layer *l = reg()->Find(id)) l->SetSize(w, h);
}
void     LayerSetZ(uint32_t id, int z) {
    if (Layer *l = reg()->Find(id)) l->SetZ(z);
}
void     LayerSetAlpha(uint32_t id, float a) {
    if (Layer *l = reg()->Find(id)) l->SetAlpha(a);
}
void     LayerSetVisible(uint32_t id, bool v) {
    if (Layer *l = reg()->Find(id)) l->SetVisible(v);
}
int      LayerQueueBuffer(uint32_t id, const void *buf, size_t len) {
    if (Layer *l = reg()->Find(id)) return l->QueueBuffer(buf, len);
    return NAME_NOT_FOUND;
}
int      LayerDequeueBuffer(uint32_t id, void *out, size_t max, size_t *len) {
    if (Layer *l = reg()->Find(id)) return l->DequeueBuffer(out, max, len);
    return NAME_NOT_FOUND;
}

} /* extern "C" */
