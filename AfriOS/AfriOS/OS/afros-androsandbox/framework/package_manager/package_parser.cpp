/*
 * package_manager/package_parser.cpp — APK / AndroidManifest.xml parser.
 *
 * Android's APK is a ZIP whose top-level entry `AndroidManifest.xml` is
 * stored in Android Binary XML format (a compact, little-endian tokenised
 * representation of the XML document). The parser walks the chunked
 * binary format: a header chunk, a string pool, a resource-id pool, and
 * a tree of START/NAMESPACE/END chunks.
 *
 * This module recognises the binary-XML magic and extracts the four
 * fields the sandbox needs: packageName, versionName, application label,
 * and the comma-separated list of <uses-permission> names. If the input
 * is a plain (text) XML or any other format, we fall back to deriving a
 * package name from the file basename so install still succeeds.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

/* Binary XML chunk types (subset). */
#define RES_XML_TYPE            0x0003
#define RES_STRING_POOL_TYPE    0x0001
#define RES_XML_START_ELEMENT   0x0102
#define RES_XML_END_ELEMENT     0x0103
#define RES_XML_RESOURCE_MAP    0x0180

/* String pool flags. */
#define UTF8_FLAG               0x100

struct AxmlHeader {
    uint16_t type;
    uint16_t header_size;
    uint32_t size;
};

struct AxmlStringPool {
    uint16_t type;
    uint16_t header_size;
    uint32_t size;
    uint32_t string_count;
    uint32_t style_count;
    uint32_t flags;
    uint32_t strings_start;
    uint32_t styles_start;
};

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Parse the binary-XML string pool into `out`. Returns false on bad data. */
static bool parse_string_pool(const uint8_t *base, size_t len,
                              std::vector<std::string> &out) {
    if (len < sizeof(AxmlStringPool)) return false;
    const AxmlStringPool *sp = (const AxmlStringPool *)base;
    uint32_t count = sp->string_count;
    if (count > 65535) return false;
    const uint8_t *offsets = base + sp->header_size;
    const uint8_t *strings = base + sp->strings_start;
    out.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        if (offsets + (i + 1) * 4 > base + len) break;
        uint32_t off = rd32(offsets + i * 4);
        if (strings + off + 4 > base + sp->size) break;
        std::string s;
        if (sp->flags & UTF8_FLAG) {
            /* UTF-8: [u8 len][u8 len2 if len==0xff][bytes] */
            uint8_t n = strings[off];
            const uint8_t *p = strings + off + 1;
            if (n == 0xff && off + 5 < sp->size) { p += 2; n = p[-2]; }
            s.assign((const char *)p, n);
        } else {
            /* UTF-16: [u16 len][u16 chars] */
            uint16_t n = rd16(strings + off);
            const uint8_t *p = strings + off + 2;
            for (uint16_t j = 0; j < n && p + 1 < base + sp->size; j++, p += 2) {
                uint16_t c = rd16(p);
                if (c < 0x80) s.push_back((char)c);
                else s.push_back('?');
            }
        }
        out.push_back(s);
    }
    return true;
}

/* Walk the chunk stream; pull attributes we care about from the
 * <manifest>, <application>, and <uses-permission> elements. */
static void walk(const uint8_t *base, size_t len,
                 const std::vector<std::string> &pool,
                 char *out_pkg, size_t pkg_max,
                 char *out_ver, size_t ver_max,
                 char *out_label, size_t label_max,
                 std::vector<std::string> &perms) {
    size_t off = 8; /* skip file header */
    while (off + 8 <= len) {
        const AxmlHeader *h = (const AxmlHeader *)(base + off);
        uint16_t type = rd16((const uint8_t *)h);
        uint32_t sz   = rd32((const uint8_t *)h + 4);
        if (sz == 0 || off + sz > len) break;
        if (type == RES_XML_START_ELEMENT && off + 36 <= len) {
            uint32_t name_idx = rd32(base + off + 16);
            uint16_t nattr    = rd16(base + off + 28);
            const uint8_t *a  = base + off + 36;
            std::string tag = (name_idx < pool.size()) ? pool[name_idx] : "";
            for (uint16_t i = 0; i < nattr && a + 20 <= base + len; i++, a += 20) {
                uint32_t ns_idx = rd32(a);
                uint32_t nm_idx = rd32(a + 4);
                uint32_t val_idx = rd32(a + 12);
                (void)ns_idx;
                std::string an = (nm_idx < pool.size()) ? pool[nm_idx] : "";
                std::string av = (val_idx < pool.size()) ? pool[val_idx] : "";
                if (tag == "manifest" && an == "package" && out_pkg[0] == 0) {
                    std::snprintf(out_pkg, pkg_max, "%s", av.c_str());
                } else if (tag == "manifest" && an == "android:versionName") {
                    std::snprintf(out_ver, ver_max, "%s", av.c_str());
                } else if (tag == "application" && an == "android:label") {
                    std::snprintf(out_label, label_max, "%s", av.c_str());
                } else if (tag == "uses-permission" && an == "android:name") {
                    perms.push_back(av);
                }
            }
        }
        off += sz;
    }
}

extern "C" int PackageParserParse(const char *apk_path,
                                  char *out_pkg, size_t pkg_max,
                                  char *out_ver, size_t ver_max,
                                  char *out_label, size_t label_max,
                                  char *out_perms, size_t perms_max) {
    if (!apk_path || !out_pkg || !out_ver || !out_label || !out_perms) return BAD_VALUE;
    out_pkg[0] = out_ver[0] = out_label[0] = out_perms[0] = 0;
    FILE *f = std::fopen(apk_path, "rb");
    if (!f) return NAME_NOT_FOUND;
    /* Read the first 64 KiB — sufficient for the manifest chunk stream. */
    unsigned char buf[65536];
    size_t n = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    if (n < 8) return BAD_VALUE;
    /* Binary XML magic: 0x00080003 (LE). */
    bool is_axml = (buf[0] == 0x03 && buf[1] == 0x00 && buf[2] == 0x08 && buf[3] == 0x00);
    std::vector<std::string> pool;
    std::vector<std::string> perms;
    if (is_axml) {
        parse_string_pool(buf + 8, n - 8, pool);
        walk(buf, n, pool, out_pkg, pkg_max, out_ver, ver_max,
             out_label, label_max, perms);
    }
    if (out_pkg[0] == 0) {
        /* Fallback: derive a package name from the basename. */
        const char *bn = std::strrchr(apk_path, '/');
        bn = bn ? bn + 1 : apk_path;
        std::snprintf(out_pkg, pkg_max, "afros.%s", bn);
        std::snprintf(out_ver, ver_max, "1.0");
        out_label[0] = 0;
    }
    /* Comma-join permissions into the output buffer. */
    size_t off = 0;
    for (auto &p : perms) {
        size_t need = p.size() + 1;
        if (off + need >= perms_max) break;
        if (off > 0) out_perms[off++] = ',';
        std::memcpy(out_perms + off, p.c_str(), p.size());
        off += p.size();
    }
    out_perms[off] = 0;
    return OK;
}
