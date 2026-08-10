#ifndef AFROS_ANDROID_SANDBOX_DEFS_H
#define AFROS_ANDROID_SANDBOX_DEFS_H

/*
 * Internal shared definitions for the AfriOS Android sandbox. This header
 * provides minimal Android-style runtime types (`status_t`, `String8`,
 * `sp<>`/`wp<>` smart pointers, IBinder etc.) so that the C++ portions of
 * the sandbox can be written in idiomatic Android fashion without dragging
 * the full platform headers in. It is consumed by the binder, dalvikvm,
 * art, dex2oat, compiler, surfaceflinger, framework and services modules.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
/* C++ standard headers — only pulled in when compiling as C++. */
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <thread>
#include <condition_variable>
extern "C" {
#endif

/* C-visible status code, mirroring Android's status_t (system/core). */
typedef int32_t status_t;

#define OK                0
#define NO_MEMORY        (-12)
#define INVALID_OPERATION (-38)
#define BAD_VALUE        (-22)
#define BAD_TYPE         (-23)
#define NAME_NOT_FOUND   (-2)
#define PERMISSION_DENIED (-1)
#define NO_INIT          (-19)
#define ALREADY_EXISTS   (-17)
#define DEAD_OBJECT      (-32)
#define FAILED_TRANSACTION (-51)
#define NOT_ENOUGH_DATA  (-61)
#define WOULD_BLOCK      (-11)
#define TIMED_OUT        (-110)
#define UNKNOWN_TRANSACTION (-74)

/* Opaque handle for a binder reference. */
typedef uint32_t binder_handle_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus

/*
 * Minimal Android-style String8 — a thin wrapper over std::string with the
 * most commonly used surface (c_str, size, set, append). The real Android
 * String8 has many more helpers; the sandbox only needs the basics.
 */
class String8 {
public:
    String8() = default;
    explicit String8(const char *s) : str_(s ? s : "") {}
    String8(const char *s, size_t len) : str_(s ? s : "", len) {}
    String8(const std::string &s) : str_(s) {}
    const char *c_str() const { return str_.c_str(); }
    size_t size() const { return str_.size(); }
    bool isEmpty() const { return str_.empty(); }
    void setTo(const char *s) { str_ = s ? s : ""; }
    void append(const char *s) { if (s) str_ += s; }
    void append(const String8 &o) { str_ += o.str_; }
    String8 &operator=(const char *s) { str_ = s ? s : ""; return *this; }
    bool operator==(const String8 &o) const { return str_ == o.str_; }
    bool operator==(const char *s) const { return s && str_ == s; }
    bool operator<(const String8 &o) const { return str_ < o.str_; }
    const std::string &std_str() const { return str_; }
private:
    std::string str_;
};

/*
 * Minimal sp<T> (strong pointer) — intrusive reference counting via
 * T::incStrong()/decStrong(). Mirrors Android's RefBase-based sp<>.
 */
template <typename T>
class sp {
public:
    sp() : p_(nullptr) {}
    sp(T *p) : p_(p) { if (p_) p_->incStrong(this); }
    sp(const sp &o) : p_(o.p_) { if (p_) p_->incStrong(this); }
    sp(sp &&o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ~sp() { if (p_) p_->decStrong(this); }
    sp &operator=(T *p) {
        if (p) p->incStrong(this);
        if (p_) p_->decStrong(this);
        p_ = p;
        return *this;
    }
    sp &operator=(const sp &o) { return *this = o.p_; }
    T &operator*() const { return *p_; }
    T *operator->() const { return p_; }
    T *get() const { return p_; }
    void clear() {
        if (p_) { p_->decStrong(this); p_ = nullptr; }
    }
    bool operator==(const sp &o) const { return p_ == o.p_; }
    explicit operator bool() const { return p_ != nullptr; }
private:
    T *p_;
};

/*
 * Minimal wp<T> (weak pointer) — stores a raw pointer; does not bump the
 * strong count. promote() returns a strong sp<T> if the object is still
 * alive.
 */
template <typename T>
class wp {
public:
    wp() : p_(nullptr) {}
    wp(T *p) : p_(p) {}
    wp(const sp<T> &o) : p_(o.get()) {}
    sp<T> promote() const {
        return sp<T>(p_); /* sandbox: object is always considered alive. */
    }
    T *unsafe_get() const { return p_; }
    void clear() { p_ = nullptr; }
    explicit operator bool() const { return p_ != nullptr; }
private:
    T *p_;
};

/*
 * Lightweight RefBase — provides intrusive refcounting for sp/wp.
 */
class RefBase {
public:
    RefBase() = default;
    virtual ~RefBase() = default;
    void incStrong(const void * /*id*/) { ref_.fetch_add(1, std::memory_order_relaxed); }
    void decStrong(const void * /*id*/) {
        if (ref_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }
    int32_t getStrongCount() const { return ref_.load(std::memory_order_relaxed); }
private:
    std::atomic<int32_t> ref_{0};
};

#endif /* __cplusplus */

#endif /* AFROS_ANDROID_SANDBOX_DEFS_H */
