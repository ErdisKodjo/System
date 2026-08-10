/*
 * activity_manager/task_record.cpp — Task record.
 *
 * A TaskRecord models a single Android task: a back stack of activities
 * that share an affinity (typically a launch mode + an application). It
 * carries the root intent (the intent that launched the task), the
 * affinity string, and a list of activities belonging to it. AMS uses
 * the affinity to decide whether a new activity should start in an
 * existing task or a new one (FLAG_ACTIVITY_NEW_TASK etc.).
 *
 * This module implements the TaskRecord object as a small POD wrapper
 * around an std::vector<string>; the AMS holds a list of TaskRecords.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <vector>
#include <string>
#include <mutex>

struct TaskRecord {
    std::mutex mu;
    std::string root_intent;
    std::string affinity;
    int         task_id;
    int         user_id;
    bool        root_only;
    std::vector<std::string> activities;
};

extern "C" void *TaskRecordCreate(const char *root_intent, const char *affinity) {
    TaskRecord *t = new TaskRecord();
    t->root_intent = root_intent ? root_intent : "";
    t->affinity    = affinity    ? affinity    : "default";
    t->task_id     = 0;
    t->user_id     = 0;
    t->root_only   = false;
    return t;
}

extern "C" void TaskRecordDestroy(void *p) {
    delete static_cast<TaskRecord *>(p);
}

extern "C" int TaskRecordAddActivity(void *p, const char *activity) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t || !activity) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(t->mu);
    /* Avoid duplicates. */
    for (auto &a : t->activities) if (a == activity) return ALREADY_EXISTS;
    t->activities.push_back(activity);
    return OK;
}

extern "C" int TaskRecordRemoveActivity(void *p, const char *activity) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t || !activity) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(t->mu);
    for (auto it = t->activities.begin(); it != t->activities.end(); ++it) {
        if (*it == activity) { t->activities.erase(it); return OK; }
    }
    return NAME_NOT_FOUND;
}

extern "C" size_t TaskRecordActivityCount(void *p) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t) return 0;
    std::lock_guard<std::mutex> lk(t->mu);
    return t->activities.size();
}

extern "C" const char *TaskRecordGetRootIntent(void *p) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    return t ? t->root_intent.c_str() : nullptr;
}

extern "C" const char *TaskRecordGetAffinity(void *p) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    return t ? t->affinity.c_str() : nullptr;
}

extern "C" int TaskRecordMatchesAffinity(void *p, const char *affinity) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t || !affinity) return 0;
    std::lock_guard<std::mutex> lk(t->mu);
    return t->affinity == affinity ? 1 : 0;
}

extern "C" void TaskRecordSetId(void *p, int id) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (t) t->task_id = id;
}

extern "C" int TaskRecordGetId(void *p) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    return t ? t->task_id : -1;
}

extern "C" size_t TaskRecordListActivities(void *p, const char **out, size_t max) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t) return 0;
    std::lock_guard<std::mutex> lk(t->mu);
    size_t n = std::min(t->activities.size(), max);
    for (size_t i = 0; i < n; i++) out[i] = t->activities[i].c_str();
    return n;
}

/* Remove every activity; the task becomes empty but remains alive. */
extern "C" void TaskRecordClear(void *p) {
    TaskRecord *t = static_cast<TaskRecord *>(p);
    if (!t) return;
    std::lock_guard<std::mutex> lk(t->mu);
    t->activities.clear();
}
