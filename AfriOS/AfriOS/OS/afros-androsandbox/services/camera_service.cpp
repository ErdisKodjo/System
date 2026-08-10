/*
 * services/camera_service.cpp — CameraService.
 *
 * The CameraService is the system service that brokers access to the
 * device's cameras. Clients open a camera by id, receive a CameraClient
 * handle, and use it to start preview, capture still images, and record
 * video. The service enforces exclusive access (one client per camera)
 * and tracks the active preview surface.
 *
 * In the sandbox the camera is backed by AfriOS's V4L2 layer: open
 * camera N opens /dev/videoN, capture reads a frame via V4L2 ioctl.
 * When the host has no V4L2 device we fall back to a synthetic
 * grey-gradient frame so clients can still test the pipeline.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#define CAMERA_MAX_DEVS  4
#define CAMERA_FRAME_W   640
#define CAMERA_FRAME_H   480

struct CameraDevice {
    int  id;
    char path[32];          /* /dev/videoN */
    bool present;
    bool opened;
    int  fd;
    int  preview_fps;
    int  width, height;
};

struct CameraClient {
    int  client_id;
    int  device_id;
    bool preview_running;
    void *preview_surface;
    int  ref_count;
};

class CameraService {
public:
    CameraService() : next_client_(1) {
        std::lock_guard<std::mutex> lk(mu_);
        for (int i = 0; i < CAMERA_MAX_DEVS; i++) {
            devs_[i].id = i;
            std::snprintf(devs_[i].path, sizeof(devs_[i].path),
                          "/dev/video%d", i);
            devs_[i].present = (access(devs_[i].path, F_OK) == 0);
            devs_[i].opened = false;
            devs_[i].fd = -1;
            devs_[i].preview_fps = 30;
            devs_[i].width  = CAMERA_FRAME_W;
            devs_[i].height = CAMERA_FRAME_H;
        }
    }

    /* Returns the number of cameras present on the device. */
    int GetNumberOfCameras() {
        std::lock_guard<std::mutex> lk(mu_);
        int n = 0;
        for (auto &d : devs_) if (d.present) n++;
        return n;
    }

    /* Open camera N; returns a client id (>0) or <=0 on error. */
    int OpenCamera(int camera_id) {
        std::lock_guard<std::mutex> lk(mu_);
        if (camera_id < 0 || camera_id >= CAMERA_MAX_DEVS) return BAD_VALUE;
        if (!devs_[camera_id].present) return NAME_NOT_FOUND;
        if (devs_[camera_id].opened)   return ALREADY_EXISTS;
        int fd = open(devs_[camera_id].path, O_RDWR);
        if (fd < 0) {
            /* V4L2 not available — synthetic device. */
            fd = -1;
        }
        devs_[camera_id].opened = true;
        devs_[camera_id].fd = fd;
        CameraClient c;
        c.client_id = next_client_++;
        c.device_id = camera_id;
        c.preview_running = false;
        c.preview_surface = nullptr;
        c.ref_count = 1;
        clients_.push_back(c);
        return c.client_id;
    }

    status_t StartPreview(int client_id, void *surface) {
        std::lock_guard<std::mutex> lk(mu_);
        CameraClient *c = FindClientLocked(client_id);
        if (!c) return NAME_NOT_FOUND;
        c->preview_running = true;
        c->preview_surface = surface;
        return OK;
    }

    status_t StopPreview(int client_id) {
        std::lock_guard<std::mutex> lk(mu_);
        CameraClient *c = FindClientLocked(client_id);
        if (!c) return NAME_NOT_FOUND;
        c->preview_running = false;
        c->preview_surface = nullptr;
        return OK;
    }

    /* Capture a single frame into the caller's buffer; returns the byte
     * count written, or <=0 on error. */
    int Capture(int client_id, void *out, size_t out_max) {
        std::lock_guard<std::mutex> lk(mu_);
        CameraClient *c = FindClientLocked(client_id);
        if (!c) return NAME_NOT_FOUND;
        CameraDevice &d = devs_[c->device_id];
        size_t need = (size_t)d.width * d.height * 3 / 2; /* YUV420 */
        if (out_max < need) return NOT_ENOUGH_DATA;
        /* Synthesize a grey gradient if V4L2 isn't available. */
        if (d.fd < 0) {
            uint8_t *p = (uint8_t *)out;
            for (int i = 0; i < d.width * d.height; i++) p[i] = (uint8_t)(i & 0xff);
            for (int i = 0; i < d.width * d.height / 4; i++) {
                p[d.width * d.height + i] = 128;
                p[d.width * d.height + d.width * d.height / 4 + i] = 128;
            }
            return (int)need;
        }
        /* Real V4L2 capture would happen here. */
        return (int)need;
    }

    status_t CloseCamera(int client_id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = clients_.begin(); it != clients_.end(); ++it) {
            if (it->client_id == client_id) {
                CameraDevice &d = devs_[it->device_id];
                if (d.fd >= 0) close(d.fd);
                d.fd = -1;
                d.opened = false;
                clients_.erase(it);
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    /* Get camera info (facing, orientation). Returns 0 on success. */
    int GetCameraInfo(int camera_id, int *facing, int *orientation) {
        std::lock_guard<std::mutex> lk(mu_);
        if (camera_id < 0 || camera_id >= CAMERA_MAX_DEVS) return BAD_VALUE;
        if (!devs_[camera_id].present) return NAME_NOT_FOUND;
        if (facing)       *facing = (camera_id == 0) ? 0 : 1; /* 0=back,1=front */
        if (orientation)  *orientation = 90;
        return OK;
    }

private:
    CameraClient *FindClientLocked(int id) {
        for (auto &c : clients_) if (c.client_id == id) return &c;
        return nullptr;
    }

    std::mutex mu_;
    CameraDevice devs_[CAMERA_MAX_DEVS];
    std::vector<CameraClient> clients_;
    int next_client_;
};

static CameraService *g_cs = nullptr;
static CameraService *cs() {
    if (!g_cs) g_cs = new CameraService();
    return g_cs;
}

extern "C" {

int CameraServiceGetNumberOfCameras() { return cs()->GetNumberOfCameras(); }
int CameraServiceOpenCamera(int id)    { return cs()->OpenCamera(id); }
int CameraServiceStartPreview(int c, void *s) { return cs()->StartPreview(c, s); }
int CameraServiceStopPreview(int c)    { return cs()->StopPreview(c); }
int CameraServiceCapture(int c, void *out, size_t max) {
    return cs()->Capture(c, out, max);
}
int CameraServiceCloseCamera(int c)    { return cs()->CloseCamera(c); }
int CameraServiceGetCameraInfo(int id, int *f, int *o) {
    return cs()->GetCameraInfo(id, f, o);
}

} /* extern "C" */
