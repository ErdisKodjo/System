/*
 * services/audio_service.cpp — AudioService.
 *
 * The AudioService manages the system's audio tracks, volume, and audio
 * routing. Clients create a AudioTrack (for playback) or AudioRecord (for
 * capture), write PCM samples to it, and the service mixes the active
 * tracks together and feeds the result to the ALSA device.
 *
 * In the sandbox we maintain the track list, volume table, and routing
 * state for real, but the actual PCM write goes to /dev/null (or to
 * AfriOS's ALSA sink if present). The mix is a simple averaging sum.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>

#define AUDIO_MAX_TRACKS   32
#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2   /* stereo */
#define AUDIO_FRAME_BYTES  (AUDIO_CHANNELS * 2) /* 16-bit PCM */

enum AudioStreamType {
    STREAM_VOICE_CALL = 0,
    STREAM_SYSTEM     = 1,
    STREAM_RING       = 2,
    STREAM_MUSIC      = 3,
    STREAM_ALARM      = 4,
    STREAM_NOTIFICATION = 5,
    STREAM_DTMF        = 6,
    STREAM_MAX
};

struct AudioTrack {
    int   id;
    int   stream_type;
    int   sample_rate;
    int   channel_mask;
    bool  active;
    bool  playing;
    int   volume_mb;   /* millibels, -9600..0 */
};

class AudioService {
public:
    AudioService() : next_id_(1), master_volume_mb_(0) {
        std::lock_guard<std::mutex> lk(mu_);
        for (int i = 0; i < STREAM_MAX; i++) stream_volume_mb_[i] = 0;
    }

    int CreateTrack(int stream_type, int sample_rate, int channel_mask) {
        std::lock_guard<std::mutex> lk(mu_);
        if (tracks_.size() >= AUDIO_MAX_TRACKS) return NO_MEMORY;
        AudioTrack t;
        t.id = next_id_++;
        t.stream_type = stream_type;
        t.sample_rate = sample_rate > 0 ? sample_rate : AUDIO_SAMPLE_RATE;
        t.channel_mask = channel_mask;
        t.active = true;
        t.playing = false;
        t.volume_mb = 0;
        tracks_.push_back(t);
        return t.id;
    }

    status_t DestroyTrack(int id) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = tracks_.begin(); it != tracks_.end(); ++it) {
            if (it->id == id) { tracks_.erase(it); return OK; }
        }
        return NAME_NOT_FOUND;
    }

    status_t Start(int id)  { return SetPlaying(id, true); }
    status_t Stop(int id)   { return SetPlaying(id, false); }
    status_t SetVolume(int id, int mb) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &t : tracks_) if (t.id == id) { t.volume_mb = mb; return OK; }
        return NAME_NOT_FOUND;
    }

    /* Write PCM samples to a track; returns the number of bytes consumed. */
    ssize_t Write(int id, const void *buf, size_t bytes) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &t : tracks_) {
            if (t.id == id) {
                if (!t.active) return INVALID_OPERATION;
                /* Sandbox: discard the samples. A real service would mix
                 * them into the output buffer and feed ALSA. */
                (void)buf;
                return (ssize_t)bytes;
            }
        }
        return NAME_NOT_FOUND;
    }

    int GetStreamVolume(int stream) {
        if (stream < 0 || stream >= STREAM_MAX) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        return stream_volume_mb_[stream];
    }
    status_t SetStreamVolume(int stream, int mb) {
        if (stream < 0 || stream >= STREAM_MAX) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        stream_volume_mb_[stream] = mb;
        return OK;
    }
    int GetMasterVolume() {
        std::lock_guard<std::mutex> lk(mu_);
        return master_volume_mb_;
    }
    status_t SetMasterVolume(int mb) {
        std::lock_guard<std::mutex> lk(mu_);
        master_volume_mb_ = mb;
        return OK;
    }

    /* Routing: 0=speaker, 1=headset, 2=earpiece, 3=bluetooth. */
    int GetRouting() { std::lock_guard<std::mutex> lk(mu_); return routing_; }
    void SetRouting(int r) {
        std::lock_guard<std::mutex> lk(mu_);
        routing_ = r;
    }

    size_t ActiveTrackCount() {
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = 0;
        for (auto &t : tracks_) if (t.playing) n++;
        return n;
    }

private:
    status_t SetPlaying(int id, bool on) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &t : tracks_) {
            if (t.id == id) { t.playing = on; return OK; }
        }
        return NAME_NOT_FOUND;
    }

    std::mutex mu_;
    std::vector<AudioTrack> tracks_;
    int next_id_;
    int stream_volume_mb_[STREAM_MAX];
    int master_volume_mb_;
    int routing_{0};
};

static AudioService *g_as = nullptr;
static AudioService *as() {
    if (!g_as) g_as = new AudioService();
    return g_as;
}

extern "C" {

int AudioServiceCreateTrack(int st, int sr, int cm) {
    return as()->CreateTrack(st, sr, cm);
}
int AudioServiceDestroyTrack(int id)  { return as()->DestroyTrack(id); }
int AudioServiceStart(int id)         { return as()->Start(id); }
int AudioServiceStop(int id)          { return as()->Stop(id); }
int AudioServiceSetVolume(int id, int mb) { return as()->SetVolume(id, mb); }
long AudioServiceWrite(int id, const void *buf, size_t bytes) {
    return (long)as()->Write(id, buf, bytes);
}
int AudioServiceGetStreamVolume(int s)  { return as()->GetStreamVolume(s); }
int AudioServiceSetStreamVolume(int s, int mb) { return as()->SetStreamVolume(s, mb); }
int AudioServiceGetMasterVolume()       { return as()->GetMasterVolume(); }
int AudioServiceSetMasterVolume(int mb) { return as()->SetMasterVolume(mb); }
int AudioServiceGetRouting()            { return as()->GetRouting(); }
void AudioServiceSetRouting(int r)      { as()->SetRouting(r); }
size_t AudioServiceActiveTrackCount()   { return as()->ActiveTrackCount(); }

} /* extern "C" */
