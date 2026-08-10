/*
 * window_manager/window_manager_service.cpp — WMS.
 *
 * The WindowManagerService tracks every window in the system: its
 * attributes (position, size, type, flags), the surface it draws to, its
 * input channel, and its place in the Z order. It computes the focused
 * window, dispatches input events to it, and notifies the activity that
 * owns the window of focus changes.
 *
 * In the sandbox we keep the model but stub out the actual surface and
 * input dispatch. The WMS still maintains a real window list, picks a
 * real focused window based on Z order + visibility, and tracks per-
 * display state via DisplayContent.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

extern "C" {
    /* WindowState (window_state.cpp). */
    void *WindowStateCreate(const char *owner, int x, int y, int w, int h, int z);
    void  WindowStateDestroy(void *ws);
    int   WindowStateSetFrame(void *ws, int x, int y, int w, int h);
    int   WindowStateSetZ(void *ws, int z);
    int   WindowStateGetZ(void *ws);
    const char *WindowStateGetOwner(void *ws);
    void  WindowStateSetVisible(void *ws, int vis);
    int   WindowStateIsVisible(void *ws);
    /* DisplayContent (display_content.cpp). */
    void *DisplayContentCreate(int display_id, int w, int h);
    void  DisplayContentDestroy(void *dc);
    int   DisplayContentAddWindow(void *dc, void *ws);
    int   DisplayContentRemoveWindow(void *dc, void *ws);
    int   DisplayContentGetRotation(void *dc);
    void  DisplayContentSetRotation(void *dc, int rot);
}

struct WindowEntry {
    void       *ws;       /* WindowState* */
    std::string owner;
    int         z;
    bool        visible;
    bool        has_focus;
};

class WindowManagerService {
public:
    WindowManagerService() : default_display_(nullptr), focused_(nullptr) {
        default_display_ = DisplayContentCreate(0, 1080, 1920);
    }
    ~WindowManagerService() {
        for (auto &e : windows_) WindowStateDestroy(e.ws);
        if (default_display_) DisplayContentDestroy(default_display_);
    }

    status_t AddWindow(const char *owner, int x, int y, int w, int h, int z,
                       void **out_token) {
        if (!owner) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        void *ws = WindowStateCreate(owner, x, y, w, h, z);
        if (!ws) return NO_MEMORY;
        WindowEntry e;
        e.ws = ws;
        e.owner = owner;
        e.z = z;
        e.visible = true;
        e.has_focus = false;
        windows_.push_back(e);
        DisplayContentAddWindow(default_display_, ws);
        RecomputeFocusLocked();
        if (out_token) *out_token = ws;
        return OK;
    }

    status_t RemoveWindow(void *token) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = windows_.begin(); it != windows_.end(); ++it) {
            if (it->ws == token) {
                DisplayContentRemoveWindow(default_display_, it->ws);
                WindowStateDestroy(it->ws);
                windows_.erase(it);
                RecomputeFocusLocked();
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    status_t SetWindowFrame(void *token, int x, int y, int w, int h) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : windows_) {
            if (e.ws == token) return WindowStateSetFrame(e.ws, x, y, w, h);
        }
        return NAME_NOT_FOUND;
    }

    status_t SetWindowZ(void *token, int z) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : windows_) {
            if (e.ws == token) {
                e.z = z;
                WindowStateSetZ(e.ws, z);
                RecomputeFocusLocked();
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    status_t SetWindowVisible(void *token, bool vis) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &e : windows_) {
            if (e.ws == token) {
                e.visible = vis;
                WindowStateSetVisible(e.ws, vis ? 1 : 0);
                RecomputeFocusLocked();
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    /* Returns the token of the currently focused window (or nullptr). */
    void *GetFocusedWindow() {
        std::lock_guard<std::mutex> lk(mu_);
        return focused_;
    }

    /* Dispatch an input event to the focused window. Returns OK if a
     * window consumed it, NAME_NOT_FOUND if there is no focused window. */
    status_t DispatchInput(const void * /*event*/, size_t /*len*/) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!focused_) return NAME_NOT_FOUND;
        return OK; /* sandbox: no real input dispatch */
    }

    size_t WindowCount() {
        std::lock_guard<std::mutex> lk(mu_);
        return windows_.size();
    }

    int GetRotation()  { return DisplayContentGetRotation(default_display_); }
    void SetRotation(int r) { DisplayContentSetRotation(default_display_, r); }

private:
    void RecomputeFocusLocked() {
        /* The highest-z visible window gets focus. */
        WindowEntry *best = nullptr;
        for (auto &e : windows_) {
            if (!e.visible) { e.has_focus = false; continue; }
            if (!best || e.z > best->z) best = &e;
            e.has_focus = false;
        }
        if (best) best->has_focus = true;
        focused_ = best ? best->ws : nullptr;
    }

    std::mutex mu_;
    void *default_display_;
    std::vector<WindowEntry> windows_;
    void *focused_;
};

static WindowManagerService *g_wms = nullptr;
static WindowManagerService *wms() {
    if (!g_wms) g_wms = new WindowManagerService();
    return g_wms;
}

extern "C" {

status_t WmsAddWindow(const char *owner, int x, int y, int w, int h, int z,
                      void **out_token) {
    return wms()->AddWindow(owner, x, y, w, h, z, out_token);
}
status_t WmsRemoveWindow(void *token) { return wms()->RemoveWindow(token); }
status_t WmsSetWindowFrame(void *t, int x, int y, int w, int h) {
    return wms()->SetWindowFrame(t, x, y, w, h);
}
status_t WmsSetWindowZ(void *t, int z)        { return wms()->SetWindowZ(t, z); }
status_t WmsSetWindowVisible(void *t, int vis){ return wms()->SetWindowVisible(t, vis != 0); }
void    *WmsGetFocusedWindow()                 { return wms()->GetFocusedWindow(); }
status_t WmsDispatchInput(const void *e, size_t l) { return wms()->DispatchInput(e, l); }
size_t   WmsWindowCount()                       { return wms()->WindowCount(); }
int      WmsGetRotation()                       { return wms()->GetRotation(); }
void     WmsSetRotation(int r)                  { wms()->SetRotation(r); }

} /* extern "C" */
