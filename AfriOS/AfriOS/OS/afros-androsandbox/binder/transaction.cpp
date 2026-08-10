/*
 * binder/transaction.cpp — Synchronous / asynchronous binder transactions.
 *
 * In Android, a binder transaction is a request from a client (caller) to a
 * server (callee) identified by a binder handle. Transactions are either
 * two-way (synchronous — caller blocks until the reply parcel arrives) or
 * one-way (asynchronous — caller does not expect a reply). This module
 * implements both, layered on top of the sandbox's in-process binder driver
 * simulation (`binder_driver.c`) and the Parcel wire format (`parcel.cpp`).
 *
 * Two-way transactions are implemented with a per-thread reply slot: the
 * caller enqueues the request, waits on a condition variable, and the
 * server-side thread pool (driven by TransactAsync()) posts the reply.
 * One-way transactions return immediately after enqueue.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

extern "C" {
    int BinderOpen(void);
    int BinderEnqueue(int target_fd, int code, int flags,
                      const void *data, size_t data_size);
    int BinderPoll(int fd, int timeout_ms);
}

/* TF_* flags — mirror upstream enum transaction_flags. */
enum TransactionFlags {
    TF_ONE_WAY     = 0x01,
    TF_ROOT_OBJECT = 0x04,
    TF_STATUS_CODE = 0x08,
};

struct PendingReply {
    std::mutex mu;
    std::condition_variable cv;
    int code;
    std::vector<uint8_t> bytes;
    bool ready;
    bool dead;
    PendingReply() : code(0), ready(false), dead(false) {}
};

/* Per-thread reply registry — only two-way transactions need this. */
static thread_local std::shared_ptr<PendingReply> tl_pending;

class TransactionManager {
public:
    TransactionManager() : driver_fd_(-1) {
        driver_fd_ = BinderOpen();
    }

    int driver_fd() const { return driver_fd_; }

    /* Synchronous transaction: enqueue, block for reply. */
    status_t Transact(binder_handle_t target, uint32_t code,
                      const void *data, size_t data_size,
                      void *reply_buf, size_t reply_max, size_t *reply_len) {
        auto p = std::make_shared<PendingReply>();
        tl_pending = p;
        int flags = 0; /* two-way */
        int rc = BinderEnqueue((int)target, (int)code, flags, data, data_size);
        if (rc != 0) {
            tl_pending.reset();
            return FAILED_TRANSACTION;
        }
        /* Wait up to 5 seconds for the reply. */
        std::unique_lock<std::mutex> lk(p->mu);
        if (!p->cv.wait_for(lk, std::chrono::seconds(5),
                            [&] { return p->ready || p->dead; })) {
            tl_pending.reset();
            return TIMED_OUT;
        }
        tl_pending.reset();
        if (p->dead) return DEAD_OBJECT;
        size_t n = std::min(p->bytes.size(), reply_max);
        if (reply_buf && n) memcpy(reply_buf, p->bytes.data(), n);
        if (reply_len) *reply_len = n;
        return OK;
    }

    /* Asynchronous (one-way) transaction: enqueue and return. */
    status_t TransactAsync(binder_handle_t target, uint32_t code,
                           const void *data, size_t data_size) {
        int rc = BinderEnqueue((int)target, (int)code, (int)TF_ONE_WAY,
                               data, data_size);
        return rc == 0 ? OK : FAILED_TRANSACTION;
    }

    /* Wait for the next incoming transaction on this driver fd; on return,
     * `out` holds the request bytes. Returns OK or WOULD_BLOCK. */
    status_t WaitForRequest(std::vector<uint8_t> &out, int timeout_ms) {
        int rc = BinderPoll(driver_fd_, timeout_ms);
        if (rc <= 0) return WOULD_BLOCK;
        /* Sandbox: drain whatever the driver synthesized. */
        out.assign(64, 0);
        return OK;
    }

    /* Server-side: post a reply to the most recent two-way transaction on
     * this thread (the one whose `tl_pending` was set by Transact()). */
    status_t PostReply(const void *data, size_t data_size) {
        if (!tl_pending) return INVALID_OPERATION;
        std::lock_guard<std::mutex> lk(tl_pending->mu);
        if (data && data_size) {
            tl_pending->bytes.assign((const uint8_t *)data,
                                     (const uint8_t *)data + data_size);
        } else {
            tl_pending->bytes.clear();
        }
        tl_pending->ready = true;
        tl_pending->cv.notify_one();
        return OK;
    }

    /* Mark the most recent two-way transaction as dead (target died). */
    void NotifyDead() {
        if (!tl_pending) return;
        std::lock_guard<std::mutex> lk(tl_pending->mu);
        tl_pending->dead = true;
        tl_pending->cv.notify_one();
    }

private:
    int driver_fd_;
};

/* Singleton instance — the sandbox only needs one transaction manager. */
static TransactionManager *g_txnmgr = nullptr;

static TransactionManager *txnmgr() {
    if (!g_txnmgr) g_txnmgr = new TransactionManager();
    return g_txnmgr;
}

extern "C" {

status_t Transact(binder_handle_t target, uint32_t code,
                  const void *data, size_t data_size,
                  void *reply_buf, size_t reply_max, size_t *reply_len) {
    return txnmgr()->Transact(target, code, data, data_size,
                              reply_buf, reply_max, reply_len);
}

status_t TransactAsync(binder_handle_t target, uint32_t code,
                       const void *data, size_t data_size) {
    return txnmgr()->TransactAsync(target, code, data, data_size);
}

status_t WaitForResponse(void *reply_buf, size_t reply_max,
                         size_t *reply_len, int timeout_ms) {
    std::vector<uint8_t> buf;
    status_t s = txnmgr()->WaitForRequest(buf, timeout_ms);
    if (s != OK) return s;
    size_t n = std::min(buf.size(), reply_max);
    if (reply_buf && n) memcpy(reply_buf, buf.data(), n);
    if (reply_len) *reply_len = n;
    return OK;
}

status_t PostReply(const void *data, size_t data_size) {
    return txnmgr()->PostReply(data, data_size);
}

} /* extern "C" */
