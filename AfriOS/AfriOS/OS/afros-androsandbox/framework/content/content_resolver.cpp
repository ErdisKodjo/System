/*
 * content/content_resolver.cpp — ContentResolver.
 *
 * ContentResolver is the client-side entry point to the content provider
 * system. Given a content:// URI (e.g. "content://com.android.contacts/
 * raw_contacts/42"), it parses out the authority ("com.android.contacts")
 * and the path ("/raw_contacts/42"), looks up the matching provider via
 * the provider registry (content_provider.cpp), and delegates the
 * query/insert/update/delete/getType call to it.
 *
 * This module also provides a per-process cache of acquired providers so
 * we don't re-do the registry lookup on every call.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <mutex>

extern "C" {
    /* Forward-declared opaque types — the real definitions live in
     * content_provider.cpp. We treat them as `void *` here to keep this
     * translation unit independent. */
    typedef void ProviderHandle;
    typedef void CursorHandle;

    ProviderHandle *ContentProviderAcquire(const char *authority);
    void            ContentProviderRelease(ProviderHandle *p);
    CursorHandle   *CursorCreate();
    void            CursorDestroy(CursorHandle *c);

    /* Provider-side C wrappers — see content_provider.cpp. */
    CursorHandle   *ContentProviderQuery(ProviderHandle *p, const char *uri,
                                         const char *const *proj, size_t n,
                                         const char *selection);
    status_t        ContentProviderInsert(ProviderHandle *p, const char *uri,
                                          const char *values);
    int             ContentProviderUpdate(ProviderHandle *p, const char *uri,
                                          const char *values,
                                          const char *selection);
    int             ContentProviderDelete(ProviderHandle *p, const char *uri,
                                          const char *selection);
    const char     *ContentProviderGetType(ProviderHandle *p, const char *uri);
}

/* Parsed pieces of a content:// URI. */
struct UriParts {
    std::string authority;   /* host part, e.g. "com.android.contacts" */
    std::string path;        /* path part, e.g. "/raw_contacts/42"     */
    std::string query;       /* everything after '?', if any           */
    std::string fragment;    /* everything after '#', if any           */
};

static bool parse_uri(const char *uri, UriParts &out) {
    if (!uri) return false;
    const char *prefix = "content://";
    if (std::strncmp(uri, prefix, std::strlen(prefix)) != 0) return false;
    const char *p = uri + std::strlen(prefix);
    const char *slash = std::strchr(p, '/');
    const char *q     = std::strchr(p, '?');
    const char *frag  = std::strchr(p, '#');
    const char *auth_end = slash;
    if (!auth_end || (q && q < auth_end) || (frag && frag < auth_end)) {
        auth_end = q ? q : (frag ? frag : p + std::strlen(p));
    }
    out.authority.assign(p, auth_end - p);
    const char *path_end = q ? q : (frag ? frag : p + std::strlen(p));
    if (slash && slash < path_end) out.path.assign(slash, path_end - slash);
    else                            out.path = "/";
    if (q) {
        const char *e = frag ? frag : p + std::strlen(p);
        out.query.assign(q + 1, e - (q + 1));
    }
    if (frag) out.fragment = frag + 1;
    return !out.authority.empty();
}

class ContentResolver {
public:
    /* Query a URI; returns a Cursor (caller must destroy) or nullptr. */
    CursorHandle *Query(const char *uri, const char *const *proj, size_t n,
                        const char *selection) {
        UriParts u;
        if (!parse_uri(uri, u)) return nullptr;
        ProviderHandle *p = Acquire(u.authority.c_str());
        if (!p) return nullptr;
        return ContentProviderQuery(p, uri, proj, n, selection);
    }

    status_t Insert(const char *uri, const char *values) {
        UriParts u;
        if (!parse_uri(uri, u)) return BAD_VALUE;
        ProviderHandle *p = Acquire(u.authority.c_str());
        if (!p) return NAME_NOT_FOUND;
        return ContentProviderInsert(p, uri, values);
    }

    int Update(const char *uri, const char *values, const char *selection) {
        UriParts u;
        if (!parse_uri(uri, u)) return BAD_VALUE;
        ProviderHandle *p = Acquire(u.authority.c_str());
        if (!p) return NAME_NOT_FOUND;
        return ContentProviderUpdate(p, uri, values, selection);
    }

    int Delete(const char *uri, const char *selection) {
        UriParts u;
        if (!parse_uri(uri, u)) return BAD_VALUE;
        ProviderHandle *p = Acquire(u.authority.c_str());
        if (!p) return NAME_NOT_FOUND;
        return ContentProviderDelete(p, uri, selection);
    }

    std::string GetType(const char *uri) {
        UriParts u;
        if (!parse_uri(uri, u)) return "";
        ProviderHandle *p = Acquire(u.authority.c_str());
        if (!p) return "";
        const char *r = ContentProviderGetType(p, uri);
        return r ? r : "";
    }

private:
    ProviderHandle *Acquire(const char *authority) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = cache_.find(authority);
        if (it != cache_.end()) return it->second;
        ProviderHandle *p = ContentProviderAcquire(authority);
        if (p) cache_[authority] = p;
        return p;
    }
    std::mutex mu_;
    std::unordered_map<std::string, ProviderHandle *> cache_;
};

static ContentResolver *g_cr = nullptr;
static ContentResolver *cr() {
    if (!g_cr) g_cr = new ContentResolver();
    return g_cr;
}

extern "C" {

CursorHandle *ContentResolverQuery(const char *uri,
                                   const char *const *proj, size_t n,
                                   const char *selection) {
    return cr()->Query(uri, proj, n, selection);
}
status_t ContentResolverInsert(const char *uri, const char *values) {
    return cr()->Insert(uri, values);
}
int ContentResolverUpdate(const char *uri, const char *values,
                          const char *selection) {
    return cr()->Update(uri, values, selection);
}
int ContentResolverDelete(const char *uri, const char *selection) {
    return cr()->Delete(uri, selection);
}
const char *ContentResolverGetType(const char *uri) {
    static thread_local std::string s;
    s = cr()->GetType(uri);
    return s.c_str();
}

} /* extern "C" */
