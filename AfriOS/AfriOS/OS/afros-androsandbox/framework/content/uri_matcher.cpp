/*
 * content/uri_matcher.cpp — URI pattern → integer code mapping.
 *
 * Android's UriMatcher is a small tree that maps a URI pattern like
 * "content://com.example.provider/items/#" to an integer code that the
 * provider uses in its switch statement. The pattern syntax:
 *
 *   *      matches any authority
 *   #      matches any number (one or more digits)
 *   *      in a path segment matches any text
 *
 * This module implements that matching engine: build the tree with
 * addURI(), then match a URI with match().
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <string>
#include <vector>
#include <mutex>

class UriMatcher {
public:
    UriMatcher(int code) : root_(new Node()) { root_->code = code; }

    /* Register a pattern → code mapping. */
    void AddURI(const char *authority, const char *path, int code) {
        std::lock_guard<std::mutex> lk(mu_);
        Node *n = root_.get();
        if (authority) n = AddChild(n, authority);
        if (path) {
            /* Split path on '/'. */
            const char *p = path;
            while (*p == '/') p++;
            while (*p) {
                const char *e = std::strchr(p, '/');
                std::string seg = e ? std::string(p, e - p) : std::string(p);
                n = AddChild(n, seg.c_str());
                if (!e) break;
                p = e + 1;
                while (*p == '/') p++;
            }
        }
        n->code = code;
    }

    /* Match a content:// URI against the tree; returns the registered code
     * or the root code (typically UriMatcher.NO_MATCH == -1). */
    int Match(const char *uri) {
        if (!uri) return root_->code;
        std::lock_guard<std::mutex> lk(mu_);
        /* Strip "content://" prefix. */
        const char *prefix = "content://";
        const char *p = std::strstr(uri, prefix);
        p = p ? p + std::strlen(prefix) : uri;
        /* Split into authority + path. */
        const char *slash = std::strchr(p, '/');
        std::string auth = slash ? std::string(p, slash - p) : std::string(p);
        Node *n = FindChild(root_.get(), auth.c_str(), /*is_auth=*/true);
        if (!n) return root_->code;
        /* Walk path segments. */
        const char *q = slash ? slash + 1 : "";
        while (*q) {
            const char *e = std::strchr(q, '/');
            std::string seg = e ? std::string(q, e - q) : std::string(q);
            Node *next = FindChild(n, seg.c_str(), /*is_auth=*/false);
            if (!next) return root_->code;
            n = next;
            if (!e) break;
            q = e + 1;
        }
        return n->code;
    }

private:
    struct Node {
        std::string text;        /* segment text, or "*" / "#" for wildcards */
        int         code{-1};    /* matched code, or -1 if no match here */
        std::vector<std::unique_ptr<Node>> children;
    };

    Node *AddChild(Node *parent, const char *seg) {
        for (auto &c : parent->children) {
            if (c->text == seg) return c.get();
        }
        auto n = std::make_unique<Node>();
        n->text = seg;
        Node *raw = n.get();
        parent->children.push_back(std::move(n));
        return raw;
    }

    Node *FindChild(Node *parent, const char *seg, bool is_auth) {
        /* First try exact match. */
        for (auto &c : parent->children) {
            if (c->text == seg) return c.get();
        }
        /* Authority "*" matches any. */
        if (is_auth) {
            for (auto &c : parent->children) {
                if (c->text == "*") return c.get();
            }
            return nullptr;
        }
        /* Path: "#" matches a number, "*" matches anything. */
        bool is_num = (*seg != 0);
        for (const char *p = seg; *p; p++) {
            if (*p < '0' || *p > '9') { is_num = false; break; }
        }
        for (auto &c : parent->children) {
            if (is_num && c->text == "#") return c.get();
            if (c->text == "*") return c.get();
        }
        return nullptr;
    }

    std::mutex mu_;
    std::unique_ptr<Node> root_;
};

static UriMatcher *g_matcher = nullptr;
static UriMatcher *matcher() {
    if (!g_matcher) g_matcher = new UriMatcher(-1);
    return g_matcher;
}

extern "C" {

void UriMatcherInit(int no_match_code) {
    /* Replace the global matcher with one whose NO_MATCH code is given. */
    if (g_matcher) delete g_matcher;
    g_matcher = new UriMatcher(no_match_code);
}
void UriMatcherAdd(const char *authority, const char *path, int code) {
    matcher()->AddURI(authority, path, code);
}
int  UriMatcherMatch(const char *uri) {
    return matcher()->Match(uri);
}

} /* extern "C" */
