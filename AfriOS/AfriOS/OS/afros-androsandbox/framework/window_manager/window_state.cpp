/*
 * window_manager/window_state.cpp — WindowState.
 *
 * WindowState is the WMS-side representation of a single window: the
 * surface it draws to, its current frame (position + size), its Z order,
 * its owner package, its visibility, and its input channel. The WMS
 * creates one WindowState per call to WindowManager.addView() and updates
 * it whenever the app repositions or resizes the window.
 *
 * In the sandbox the "surface" is an opaque token — there is no real
 * SurfaceFlinger connection — but every other piece of state is real.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <mutex>

struct WindowState {
    std::mutex mu;
    std::string owner;       /* package/component that owns this window */
    int  x, y, w, h;         /* current frame */
    int  z;                  /* Z order */
    int  type;               /* window type (1=app, 2=system, 3=input, ...) */
    int  flags;              /* WindowManager.LayoutParams.FLAG_* */
    bool visible;
    bool has_focus;
    void *surface;           /* opaque surface token */
    int  input_channel_id;
};

extern "C" void *WindowStateCreate(const char *owner, int x, int y, int w, int h, int z) {
    WindowState *s = new WindowState();
    s->owner = owner ? owner : "";
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->z = z;
    s->type = 1;
    s->flags = 0;
    s->visible = true;
    s->has_focus = false;
    s->surface = nullptr;
    s->input_channel_id = -1;
    return s;
}

extern "C" void WindowStateDestroy(void *p) {
    delete static_cast<WindowState *>(p);
}

extern "C" int WindowStateSetFrame(void *p, int x, int y, int w, int h) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(s->mu);
    s->x = x; s->y = y; s->w = w; s->h = h;
    return OK;
}

extern "C" int WindowStateGetFrame(void *p, int *x, int *y, int *w, int *h) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(s->mu);
    if (x) *x = s->x;
    if (y) *y = s->y;
    if (w) *w = s->w;
    if (h) *h = s->h;
    return OK;
}

extern "C" int WindowStateSetZ(void *p, int z) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(s->mu);
    s->z = z;
    return OK;
}

extern "C" int WindowStateGetZ(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return s ? s->z : 0;
}

extern "C" const char *WindowStateGetOwner(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return s ? s->owner.c_str() : nullptr;
}

extern "C" void WindowStateSetVisible(void *p, int vis) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return;
    std::lock_guard<std::mutex> lk(s->mu);
    s->visible = vis != 0;
}

extern "C" int WindowStateIsVisible(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return (s && s->visible) ? 1 : 0;
}

extern "C" void WindowStateSetHasFocus(void *p, int foc) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return;
    std::lock_guard<std::mutex> lk(s->mu);
    s->has_focus = foc != 0;
}

extern "C" int WindowStateHasFocus(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return (s && s->has_focus) ? 1 : 0;
}

extern "C" void WindowStateSetSurface(void *p, void *surface) {
    WindowState *s = static_cast<WindowState *>(p);
    if (!s) return;
    std::lock_guard<std::mutex> lk(s->mu);
    s->surface = surface;
}

extern "C" void *WindowStateGetSurface(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return s ? s->surface : nullptr;
}

extern "C" void WindowStateSetType(void *p, int type) {
    WindowState *s = static_cast<WindowState *>(p);
    if (s) s->type = type;
}

extern "C" int WindowStateGetType(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return s ? s->type : 0;
}

extern "C" void WindowStateSetFlags(void *p, int flags) {
    WindowState *s = static_cast<WindowState *>(p);
    if (s) { std::lock_guard<std::mutex> lk(s->mu); s->flags = flags; }
}

extern "C" int WindowStateGetFlags(void *p) {
    WindowState *s = static_cast<WindowState *>(p);
    return s ? s->flags : 0;
}
