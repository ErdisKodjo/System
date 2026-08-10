/*
 * window_manager/display_content.cpp — Per-display state.
 *
 * A DisplayContent carries everything the WMS needs to know about one
 * physical display: the list of windows currently attached to it, its
 * current rotation, its pixel size, and its logical density. The default
 * display (DisplayContent.DEFAULT_DISPLAY, id == 0) always exists;
 * secondary displays (external, virtual) are created on demand.
 *
 * This module is intentionally small — the WMS owns the policy decisions
 * (which window is on top, which has focus); DisplayContent is just a bag
 * of state with a few helpers to enumerate windows.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <mutex>

struct DisplayContent {
    std::mutex mu;
    int  display_id;
    int  width;
    int  height;
    int  rotation;       /* 0, 90, 180, 270 */
    int  density_dpi;    /* e.g. 420 */
    std::vector<void *> windows;   /* WindowState* */
};

extern "C" void *DisplayContentCreate(int display_id, int w, int h) {
    DisplayContent *d = new DisplayContent();
    d->display_id = display_id;
    d->width = w > 0 ? w : 1080;
    d->height = h > 0 ? h : 1920;
    d->rotation = 0;
    d->density_dpi = 420;
    return d;
}

extern "C" void DisplayContentDestroy(void *p) {
    delete static_cast<DisplayContent *>(p);
}

extern "C" int DisplayContentAddWindow(void *p, void *ws) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (!d || !ws) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(d->mu);
    for (auto &w : d->windows) if (w == ws) return ALREADY_EXISTS;
    d->windows.push_back(ws);
    return OK;
}

extern "C" int DisplayContentRemoveWindow(void *p, void *ws) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (!d || !ws) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(d->mu);
    for (auto it = d->windows.begin(); it != d->windows.end(); ++it) {
        if (*it == ws) { d->windows.erase(it); return OK; }
    }
    return NAME_NOT_FOUND;
}

extern "C" size_t DisplayContentWindowCount(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (!d) return 0;
    std::lock_guard<std::mutex> lk(d->mu);
    return d->windows.size();
}

extern "C" int DisplayContentGetRotation(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    return d ? d->rotation : 0;
}

extern "C" void DisplayContentSetRotation(void *p, int rot) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (!d) return;
    /* Normalise to {0, 90, 180, 270}. */
    rot = ((rot % 360) + 360) % 360;
    if (rot == 90 || rot == 270) {
        std::lock_guard<std::mutex> lk(d->mu);
        std::swap(d->width, d->height);
    }
    d->rotation = rot;
}

extern "C" int DisplayContentGetWidth(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    return d ? d->width : 0;
}

extern "C" int DisplayContentGetHeight(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    return d ? d->height : 0;
}

extern "C" void DisplayContentSetDensity(void *p, int dpi) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (d && dpi > 0) d->density_dpi = dpi;
}

extern "C" int DisplayContentGetDensity(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    return d ? d->density_dpi : 0;
}

extern "C" int DisplayContentGetId(void *p) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    return d ? d->display_id : -1;
}

/* Enumerate windows on this display. */
extern "C" size_t DisplayContentListWindows(void *p, void **out, size_t max) {
    DisplayContent *d = static_cast<DisplayContent *>(p);
    if (!d) return 0;
    std::lock_guard<std::mutex> lk(d->mu);
    size_t n = std::min(d->windows.size(), max);
    for (size_t i = 0; i < n; i++) out[i] = d->windows[i];
    return n;
}
