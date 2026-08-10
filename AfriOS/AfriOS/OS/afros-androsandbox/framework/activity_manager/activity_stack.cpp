/*
 * activity_manager/activity_stack.cpp — Activity stack.
 *
 * Each TaskRecord owns an ActivityStack — an LIFO of ActivityRecord
 * objects representing the activities that the user has navigated
 * through. The top of the stack is the currently resumed activity;
 * the next one down is the paused activity. Pushing a new activity
 * pauses the current top, pushes the new one, and resumes it; popping
 * finishes the top, resumes the new top.
 *
 * This module implements the stack and its lifecycle hooks (pause /
 * resume / stop / destroy) as small virtual functions so the sandbox
 * can validate the call order even without real activity code.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <string>
#include <mutex>

struct ActivityRecord {
    std::string activity;     /* component name, e.g. "com.foo/.MainActivity" */
    std::string intent_action;
    int         state;        /* 0=created,1=started,2=resumed,3=paused,4=stopped,5=destroyed */
    int         launch_mode;  /* 0=standard,1=singleTop,2=singleTask,3=singleInstance */
};

class ActivityStack {
public:
    /* Push an activity on top; pause the previous top, resume the new one. */
    int Push(const char *activity, const char *intent) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!activities_.empty()) {
            activities_.back().state = 3; /* paused */
        }
        ActivityRecord r;
        r.activity = activity ? activity : "";
        r.intent_action = intent ? intent : "";
        r.state = 2; /* resumed */
        r.launch_mode = 0;
        activities_.push_back(r);
        return OK;
    }

    /* Pop the top activity (finishes it). */
    int Pop() {
        std::lock_guard<std::mutex> lk(mu_);
        if (activities_.empty()) return NAME_NOT_FOUND;
        activities_.back().state = 5; /* destroyed */
        activities_.pop_back();
        if (!activities_.empty()) activities_.back().state = 2; /* resumed */
        return OK;
    }

    /* Resume whatever is on top (used after a Pop or a configuration change). */
    int ResumeTop() {
        std::lock_guard<std::mutex> lk(mu_);
        if (activities_.empty()) return NAME_NOT_FOUND;
        activities_.back().state = 2;
        return OK;
    }

    int PauseTop() {
        std::lock_guard<std::mutex> lk(mu_);
        if (activities_.empty()) return NAME_NOT_FOUND;
        activities_.back().state = 3;
        return OK;
    }

    /* Find an activity by intent action. Returns its index or -1. */
    int FindByIntent(const char *intent) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!intent) return -1;
        for (int i = (int)activities_.size() - 1; i >= 0; --i) {
            if (activities_[i].intent_action == intent) return i;
        }
        return -1;
    }

    /* Bring the activity at `idx` to the top (used by singleTask launch). */
    int BringToFront(int idx) {
        std::lock_guard<std::mutex> lk(mu_);
        if (idx < 0 || idx >= (int)activities_.size()) return BAD_VALUE;
        ActivityRecord r = activities_[idx];
        activities_.erase(activities_.begin() + idx);
        if (!activities_.empty()) activities_.back().state = 3;
        r.state = 2;
        activities_.push_back(r);
        return OK;
    }

    size_t Depth() {
        std::lock_guard<std::mutex> lk(mu_);
        return activities_.size();
    }

    /* Snapshot the top N activities into `out`. */
    size_t TopN(int n, const char **out, size_t max) {
        std::lock_guard<std::mutex> lk(mu_);
        size_t filled = 0;
        for (int i = (int)activities_.size() - 1; i >= 0 && (int)filled < n && filled < max; --i) {
            out[filled++] = activities_[i].activity.c_str();
        }
        return filled;
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mu_);
        activities_.clear();
    }

private:
    std::mutex mu_;
    std::vector<ActivityRecord> activities_;
};

extern "C" {

void *ActivityStackCreate()                 { return new ActivityStack(); }
void  ActivityStackDestroy(void *p)         { delete static_cast<ActivityStack *>(p); }
int   ActivityStackPush(void *p, const char *a, const char *i) {
    return static_cast<ActivityStack *>(p)->Push(a, i);
}
int   ActivityStackPop(void *p)             { return static_cast<ActivityStack *>(p)->Pop(); }
int   ActivityStackResumeTop(void *p)       { return static_cast<ActivityStack *>(p)->ResumeTop(); }
int   ActivityStackPauseTop(void *p)        { return static_cast<ActivityStack *>(p)->PauseTop(); }
int   ActivityStackFindByIntent(void *p, const char *i) {
    return static_cast<ActivityStack *>(p)->FindByIntent(i);
}
int   ActivityStackBringToFront(void *p, int idx) {
    return static_cast<ActivityStack *>(p)->BringToFront(idx);
}
size_t ActivityStackDepth(void *p)          { return static_cast<ActivityStack *>(p)->Depth(); }
size_t ActivityStackTopN(void *p, int n, const char **out, size_t max) {
    return static_cast<ActivityStack *>(p)->TopN(n, out, max);
}
void  ActivityStackClear(void *p)           { static_cast<ActivityStack *>(p)->Clear(); }

} /* extern "C" */
