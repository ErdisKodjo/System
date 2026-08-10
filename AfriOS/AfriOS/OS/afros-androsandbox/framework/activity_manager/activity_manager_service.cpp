/*
 * activity_manager/activity_manager_service.cpp — AMS.
 *
 * The ActivityManagerService is the central runtime service that tracks
 * every activity, service, broadcast receiver, and content provider in
 * the system. It owns the task stack, the foreground process priority
 * list, and the OOM killer thresholds. This module models the activity
 * and service side of AMS:
 *
 *   - startActivity(intent)  — resolve the target activity, push it on the
 *     current task's activity stack, schedule the lifecycle transitions.
 *   - bindService(intent)    — resolve the target service, schedule
 *     onCreate + onBind, return a binder handle to the caller.
 *   - broadcastIntent(intent) — walk the broadcast receiver list, schedule
 *     onReceive() on each.
 *
 * The implementation keeps the state in-process; the actual lifecycle
 * callbacks are no-ops (the sandbox has no real activity implementation).
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

extern "C" {
    /* Activity stack (activity_stack.cpp). */
    void *ActivityStackCreate();
    void  ActivityStackDestroy(void *stack);
    int   ActivityStackPush(void *stack, const char *activity, const char *intent);
    int   ActivityStackPop(void *stack);
    int   ActivityStackResumeTop(void *stack);
    int   ActivityStackPauseTop(void *stack);
    size_t ActivityStackDepth(void *stack);
    /* Task record (task_record.cpp). */
    void *TaskRecordCreate(const char *root_intent, const char *affinity);
    void  TaskRecordDestroy(void *task);
    int   TaskRecordAddActivity(void *task, const char *activity);
    int   TaskRecordRemoveActivity(void *task, const char *activity);
}

struct Intent {
    std::string action;
    std::string component;
    std::string data;
    std::vector<std::string> categories;
};

struct ServiceRecord {
    std::string component;
    void       *binder;
    bool        running;
    int         connections;
};

class ActivityManagerService {
public:
    ActivityManagerService() : cur_task_(nullptr), cur_stack_(nullptr) {
        cur_stack_ = ActivityStackCreate();
        cur_task_  = TaskRecordCreate("android.intent.action.MAIN",
                                      "default");
    }
    ~ActivityManagerService() {
        if (cur_stack_) ActivityStackDestroy(cur_stack_);
        if (cur_task_)  TaskRecordDestroy(cur_task_);
    }

    status_t StartActivity(const Intent &intent) {
        std::lock_guard<std::mutex> lk(mu_);
        std::string act = intent.component;
        if (act.empty()) act = "<unknown>";
        ActivityStackPush(cur_stack_, act.c_str(), intent.action.c_str());
        TaskRecordAddActivity(cur_task_, act.c_str());
        ActivityStackResumeTop(cur_stack_);
        stats_.started++;
        return OK;
    }

    status_t FinishActivity(const char *activity) {
        if (!activity) return BAD_VALUE;
        std::lock_guard<std::mutex> lk(mu_);
        ActivityStackPop(cur_stack_);
        TaskRecordRemoveActivity(cur_task_, activity);
        ActivityStackResumeTop(cur_stack_);
        stats_.finished++;
        return OK;
    }

    status_t BindService(const Intent &intent, void **out_binder) {
        std::lock_guard<std::mutex> lk(mu_);
        ServiceRecord &s = services_[intent.component];
        if (!s.running) {
            s.component = intent.component;
            s.running = true;
            s.connections = 0;
            stats_.service_started++;
        }
        s.connections++;
        if (out_binder) *out_binder = s.binder;
        return OK;
    }

    status_t UnbindService(const char *component) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = services_.find(component);
        if (it == services_.end()) return NAME_NOT_FOUND;
        if (it->second.connections > 0) it->second.connections--;
        if (it->second.connections == 0) {
            it->second.running = false;
            stats_.service_stopped++;
        }
        return OK;
    }

    status_t BroadcastIntent(const Intent &intent) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto &cb : receivers_) {
            if (cb.action == intent.action || cb.action == "*")
                cb.fn(intent.action.c_str());
        }
        stats_.broadcasts++;
        return OK;
    }

    using ReceiverFn = void (*)(const char *action);
    status_t RegisterReceiver(const char *action, ReceiverFn fn) {
        std::lock_guard<std::mutex> lk(mu_);
        receivers_.push_back({action ? action : "*", fn});
        return OK;
    }

    size_t StackDepth() {
        return ActivityStackDepth(cur_stack_);
    }

private:
    struct Receiver { std::string action; ReceiverFn fn; };
    struct Stats {
        size_t started{0}, finished{0};
        size_t service_started{0}, service_stopped{0};
        size_t broadcasts{0};
    } stats_;

    std::mutex mu_;
    void *cur_task_;
    void *cur_stack_;
    std::unordered_map<std::string, ServiceRecord> services_;
    std::vector<Receiver> receivers_;
};

static ActivityManagerService *g_ams = nullptr;
static ActivityManagerService *ams() {
    if (!g_ams) g_ams = new ActivityManagerService();
    return g_ams;
}

extern "C" {

status_t AmsStartActivity(const char *action, const char *component,
                          const char *data) {
    Intent i;
    i.action = action ? action : "";
    i.component = component ? component : "";
    i.data = data ? data : "";
    return ams()->StartActivity(i);
}
status_t AmsFinishActivity(const char *activity) {
    return ams()->FinishActivity(activity);
}
status_t AmsBindService(const char *component, void **out_binder) {
    Intent i; i.component = component ? component : "";
    return ams()->BindService(i, out_binder);
}
status_t AmsUnbindService(const char *component) {
    return ams()->UnbindService(component);
}
status_t AmsBroadcastIntent(const char *action, const char *component) {
    Intent i; i.action = action ? action : "";
    i.component = component ? component : "";
    return ams()->BroadcastIntent(i);
}
size_t AmsActivityStackDepth() { return ams()->StackDepth(); }

} /* extern "C" */
