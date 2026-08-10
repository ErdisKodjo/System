/*
 * content/content_provider.cpp — ContentProvider base + Cursor.
 *
 * A ContentProvider is the Android abstraction for an app-private data
 * source that other apps can query via a content:// URI. Subclasses
 * implement query/insert/update/delete/openFile; the framework exposes
 * them to clients via ContentResolver.
 *
 * This module provides:
 *   - The ContentProvider base class with virtual lifecycle hooks
 *     (onCreate, query, insert, update, delete, getType).
 *   - A simple in-memory Cursor implementation backed by a vector of
 *     vector<string> rows.
 *   - A provider registry: providers register themselves under an
 *     authority (the host part of a content:// URI); the ContentResolver
 * *     looks them up by authority and delegates calls.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

class Cursor {
public:
    Cursor() = default;

    void SetColumns(const std::vector<std::string> &cols) { columns_ = cols; }
    size_t AddRow(const std::vector<std::string> &row) {
        std::lock_guard<std::mutex> lk(mu_);
        rows_.push_back(row);
        return rows_.size();
    }

    size_t GetCount() {
        std::lock_guard<std::mutex> lk(mu_);
        return rows_.size();
    }
    size_t GetColumnCount() { return columns_.size(); }
    std::string GetColumnName(size_t i) {
        return i < columns_.size() ? columns_[i] : std::string();
    }
    int GetColumnIndex(const char *name) {
        if (!name) return -1;
        for (size_t i = 0; i < columns_.size(); i++) {
            if (columns_[i] == name) return (int)i;
        }
        return -1;
    }

    /* Move the cursor to row `pos`; returns OK or BAD_VALUE. */
    status_t MoveToPosition(int pos) {
        std::lock_guard<std::mutex> lk(mu_);
        if (pos < 0 || (size_t)pos >= rows_.size()) return BAD_VALUE;
        pos_ = pos;
        return OK;
    }
    status_t MoveToFirst() { return MoveToPosition(0); }
    status_t MoveToNext()  { return MoveToPosition(pos_ + 1); }

    std::string GetString(int col) {
        std::lock_guard<std::mutex> lk(mu_);
        if (pos_ < 0 || (size_t)pos_ >= rows_.size()) return "";
        if (col < 0 || (size_t)col >= rows_[pos_].size()) return "";
        return rows_[pos_][col];
    }

    void Close() {
        std::lock_guard<std::mutex> lk(mu_);
        rows_.clear();
        columns_.clear();
        pos_ = -1;
    }

private:
    std::mutex mu_;
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
    int pos_{-1};
};

class ContentProvider {
public:
    virtual ~ContentProvider() = default;
    virtual status_t OnCreate() { return OK; }
    virtual Cursor *Query(const char *uri, const char *const *projection,
                          size_t proj_n, const char *selection) {
        (void)uri; (void)projection; (void)proj_n; (void)selection;
        return new Cursor();
    }
    virtual status_t Insert(const char *uri, const char * /*values*/) {
        (void)uri;
        return OK;
    }
    virtual int Update(const char *uri, const char * /*values*/,
                       const char * /*selection*/) {
        (void)uri;
        return 0;
    }
    virtual int Delete(const char *uri, const char * /*selection*/) {
        (void)uri;
        return 0;
    }
    virtual std::string GetType(const char *uri) {
        (void)uri;
        return "vnd.android.cursor.dir/item";
    }
};

/* Authority → provider registry. */
static std::mutex g_reg_mu;
static std::unordered_map<std::string, ContentProvider *> g_providers;

extern "C" {

typedef ContentProvider *(*ProviderFactory)();

status_t ContentProviderRegister(const char *authority, ContentProvider *p) {
    if (!authority || !p) return BAD_VALUE;
    std::lock_guard<std::mutex> lk(g_reg_mu);
    if (g_providers.find(authority) != g_providers.end()) return ALREADY_EXISTS;
    p->OnCreate();
    g_providers[authority] = p;
    return OK;
}

ContentProvider *ContentProviderAcquire(const char *authority) {
    if (!authority) return nullptr;
    std::lock_guard<std::mutex> lk(g_reg_mu);
    auto it = g_providers.find(authority);
    return it != g_providers.end() ? it->second : nullptr;
}

void ContentProviderRelease(ContentProvider * /*p*/) {
    /* Sandbox: providers are process-singletons; nothing to release. */
}

/* Provider-side C wrappers — used by content_resolver.cpp so it doesn't
 * have to know the ContentProvider vtable layout. */
Cursor *ContentProviderQuery(ContentProvider *p, const char *uri,
                             const char *const *proj, size_t proj_n,
                             const char *selection) {
    return p ? p->Query(uri, proj, proj_n, selection) : nullptr;
}
status_t ContentProviderInsert(ContentProvider *p, const char *uri,
                               const char *values) {
    return p ? p->Insert(uri, values) : NAME_NOT_FOUND;
}
int ContentProviderUpdate(ContentProvider *p, const char *uri,
                          const char *values, const char *selection) {
    return p ? p->Update(uri, values, selection) : NAME_NOT_FOUND;
}
int ContentProviderDelete(ContentProvider *p, const char *uri,
                          const char *selection) {
    return p ? p->Delete(uri, selection) : NAME_NOT_FOUND;
}
const char *ContentProviderGetType(ContentProvider *p, const char *uri) {
    static thread_local std::string s;
    s = p ? p->GetType(uri) : std::string();
    return s.c_str();
}

/* Cursor C wrappers — used by content_resolver.cpp. */
typedef Cursor *CursorHandle;

CursorHandle CursorCreate() { return new Cursor(); }
void   CursorDestroy(CursorHandle c) { if (c) { c->Close(); delete c; } }
size_t CursorGetCount(CursorHandle c) { return c ? c->GetCount() : 0; }
int    CursorGetColumnIndex(CursorHandle c, const char *n) {
    return c ? c->GetColumnIndex(n) : -1;
}
status_t CursorMoveToFirst(CursorHandle c) { return c ? c->MoveToFirst() : BAD_VALUE; }
status_t CursorMoveToNext(CursorHandle c)  { return c ? c->MoveToNext()  : BAD_VALUE; }
const char *CursorGetString(CursorHandle c, int col) {
    static thread_local std::string s;
    s = c ? c->GetString(col) : "";
    return s.c_str();
}

} /* extern "C" */
