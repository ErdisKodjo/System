/*
 * services/notification_service.cpp — NotificationManager.
 *
 * The NotificationManager receives notifications posted by apps (via
 * NotificationManager.notify()), keeps the active set, posts them to the
 * system UI (status bar), and dismisses them on user action or cancel().
 * Each notification belongs to a channel (NotificationChannel) which the
 * app must create before posting. Channels carry an importance level
 * (NONE/MIN/LOW/DEFAULT/HIGH/MAX) which controls whether the system UI
 * makes a sound, peeks the notification, etc.
 *
 * In the sandbox the manager maintains the channel table and the active
 * notification list for real; UI is a no-op.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>

enum Importance {
    IMPORTANCE_NONE   = 0,
    IMPORTANCE_MIN    = 1,
    IMPORTANCE_LOW    = 2,
    IMPORTANCE_DEFAULT= 3,
    IMPORTANCE_HIGH   = 4,
    IMPORTANCE_MAX    = 5,
};

struct NotificationChannel {
    std::string id;          /* channel id */
    std::string name;        /* user-visible name */
    std::string description;
    int         importance;
    bool        sound;
    bool        vibration;
    bool        lights;
};

struct NotificationRecord {
    int         id;          /* per-app notification id */
    std::string pkg;         /* posting package */
    std::string channel_id;
    std::string title;
    std::string text;
    int64_t     posted_ns;
    bool        ongoing;
    bool        auto_cancel;
};

class NotificationManager {
public:
    NotificationManager() : next_key_(1) {}

    status_t CreateChannel(const char *pkg, const NotificationChannel &ch) {
        if (!pkg) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        std::string key = MakeKey(pkg, ch.id);
        if (channels_.find(key) != channels_.end()) return ALREADY_EXISTS;
        channels_[key] = ch;
        return OK;
    }

    status_t DeleteChannel(const char *pkg, const char *channel_id) {
        if (!pkg || !channel_id) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        std::string key = MakeKey(pkg, channel_id);
        auto it = channels_.find(key);
        if (it == channels_.end()) return NAME_NOT_FOUND;
        channels_.erase(it);
        return OK;
    }

    int Post(const char *pkg, int id, const char *channel_id,
             const char *title, const char *text, bool ongoing) {
        if (!pkg || !channel_id) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        std::string key = MakeKey(pkg, channel_id);
        if (channels_.find(key) == channels_.end()) {
            /* Auto-create a DEFAULT-importance channel if app forgot. */
            NotificationChannel ch;
            ch.id = channel_id;
            ch.name = channel_id;
            ch.importance = IMPORTANCE_DEFAULT;
            ch.sound = ch.vibration = ch.lights = true;
            channels_[key] = ch;
        }
        /* Replace existing (pkg, id) if present. */
        for (auto &n : notifications_) {
            if (n.pkg == pkg && n.id == id) {
                n.channel_id = channel_id;
                n.title = title ? title : "";
                n.text = text ? text : "";
                n.ongoing = ongoing;
                n.posted_ns = NowNs();
                return OK;
            }
        }
        NotificationRecord n;
        n.id = id;
        n.pkg = pkg;
        n.channel_id = channel_id;
        n.title = title ? title : "";
        n.text = text ? text : "";
        n.ongoing = ongoing;
        n.auto_cancel = !ongoing;
        n.posted_ns = NowNs();
        notifications_.push_back(n);
        posted_count_++;
        return OK;
    }

    status_t Cancel(const char *pkg, int id) {
        if (!pkg) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
            if (it->pkg == pkg && it->id == id) {
                notifications_.erase(it);
                cancelled_count_++;
                return OK;
            }
        }
        return NAME_NOT_FOUND;
    }

    status_t CancelAll(const char *pkg) {
        if (!pkg) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        size_t before = notifications_.size();
        for (auto it = notifications_.begin(); it != notifications_.end();) {
            if (it->pkg == pkg) it = notifications_.erase(it);
            else ++it;
        }
        cancelled_count_ += (int)(before - notifications_.size());
        return OK;
    }

    size_t ActiveCount() {
        std::lock_guard<std::mutex> lk(mu_);
        return notifications_.size();
    }

    size_t ListForPackage(const char *pkg, const char **out_titles, size_t max) {
        if (!pkg) return 0;
        std::lock_guard<std::mutex> lk(mu_);
        size_t n = 0;
        for (auto &nt : notifications_) {
            if (n >= max) break;
            if (nt.pkg == pkg) out_titles[n++] = nt.title.c_str();
        }
        return n;
    }

private:
    static std::string MakeKey(const char *pkg, const std::string &ch) {
        return std::string(pkg) + "/" + ch;
    }
    static int64_t NowNs() {
        return (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::mutex mu_;
    std::unordered_map<std::string, NotificationChannel> channels_;
    std::vector<NotificationRecord> notifications_;
    std::atomic<int> posted_count_{0};
    std::atomic<int> cancelled_count_{0};
    int next_key_;
};

static NotificationManager *g_nm = nullptr;
static NotificationManager *nm() {
    if (!g_nm) g_nm = new NotificationManager();
    return g_nm;
}

extern "C" {

/* C-side wrappers — NotificationChannel/Record are passed via simple
 * string/int parameters to keep the ABI flat. */
int  NotifCreateChannel(const char *pkg, const char *id, const char *name,
                        int importance) {
    NotificationChannel ch;
    ch.id = id ? id : "";
    ch.name = name ? name : "";
    ch.importance = importance;
    ch.sound = ch.vibration = ch.lights = (importance >= IMPORTANCE_DEFAULT);
    return nm()->CreateChannel(pkg, ch);
}
int  NotifDeleteChannel(const char *pkg, const char *id) {
    return nm()->DeleteChannel(pkg, id);
}
int  NotifPost(const char *pkg, int id, const char *channel,
               const char *title, const char *text, int ongoing) {
    return nm()->Post(pkg, id, channel, title, text, ongoing != 0);
}
int  NotifCancel(const char *pkg, int id) { return nm()->Cancel(pkg, id); }
int  NotifCancelAll(const char *pkg)       { return nm()->CancelAll(pkg); }
size_t NotifActiveCount()                  { return nm()->ActiveCount(); }
size_t NotifListForPackage(const char *pkg, const char **out, size_t max) {
    return nm()->ListForPackage(pkg, out, max);
}

} /* extern "C" */
