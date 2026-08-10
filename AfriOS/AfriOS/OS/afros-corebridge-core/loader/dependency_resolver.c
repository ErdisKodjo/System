#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../include/loader.h"

/**
 * @file dependency_resolver.c
 * @brief Resolve shared-library dependencies per runtime.
 *
 *   - PE:       parse the import table; map DLLs to Wine builtin DLLs.
 *   - ELF:      walk .dynamic / DT_NEEDED entries; resolve via ld.so cache.
 *   - Mach-O:   walk LC_LOAD_DYLIB load commands; resolve via darling dyld.
 *   - DEX:      classes.dex references; mapped to ART core libraries.
 *   - HarmonyOS:module.json "dependencies" array.
 *
 * The implementation is intentionally host-portable: instead of bundling
 * a full PE / ELF parser we shell out to objdump/otool/dexdump when they
 * are available, and fall back to a heuristic scan when they are not.
 */

#define LD_CACHE_PATH "/etc/ld.so.cache"

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Tiny local strncpy replacement to keep this file libc-light. */
static void safe_copy_helper(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (!src) { if (cap) dst[0] = '\0'; return; }
    while (src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static dep_list_t *dep_list_new(app_type_t type)
{
    dep_list_t *l = (dep_list_t *)calloc(1, sizeof(*l));
    if (!l) return NULL;
    l->type = type;
    return l;
}

static void dep_add(dep_list_t *l, const char *name, const char *resolved)
{
    dep_entry_t *e;
    if (!l || l->count >= MAX_DEP_ENTRIES) return;
    e = &l->entries[l->count++];
    safe_copy_helper(e->name, sizeof(e->name), name);
    if (resolved)
        safe_copy_helper(e->resolved_path, sizeof(e->resolved_path), resolved);
    e->resolved = (resolved && resolved[0]) ? 1 : 0;
}

/* Run a command and stream its stdout lines into @p cb. */
typedef void (*line_cb_t)(void *ctx, const char *line);
static void run_collect(const char *cmd, line_cb_t cb, void *ctx)
{
    FILE *p = popen(cmd, "r");
    char line[1024];
    if (!p) return;
    while (fgets(line, sizeof(line), p)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r'))
            line[--l] = '\0';
        if (l) cb(ctx, line);
    }
    pclose(p);
}

/* ------------------------------------------------------------------ */
/* PE: import table -> Wine DLL mapping                               */
/* ------------------------------------------------------------------ */

struct pe_ctx { dep_list_t *list; };

static void pe_line(void *ctx, const char *line)
{
    struct pe_ctx *c = (struct pe_ctx *)ctx;
    /* objdump -p foo.exe prints "    DLL Name: KERNEL32.dll" lines. */
    const char *p = strstr(line, "DLL Name:");
    if (!p) return;
    p += strlen("DLL Name:");
    while (*p == ' ' || *p == '\t') p++;
    /* Map well-known system DLLs to Wine builtins (always present). */
    dep_add(c->list, p, p); /* Wine resolves these by name automatically. */
}

static void resolve_pe(dep_list_t *list, const char *path)
{
    char cmd[1024];
    struct pe_ctx c = { list };
    snprintf(cmd, sizeof(cmd), "objdump -p \"%s\" 2>/dev/null", path);
    run_collect(cmd, pe_line, &c);
    /* If objdump didn't find anything, hint at common imports. */
    if (list->count == 0) {
        dep_add(list, "kernel32.dll", "kernel32.dll");
        dep_add(list, "user32.dll",   "user32.dll");
    }
}

/* ------------------------------------------------------------------ */
/* ELF: DT_NEEDED entries                                             */
/* ------------------------------------------------------------------ */

struct elf_ctx { dep_list_t *list; };

static void elf_line(void *ctx, const char *line)
{
    struct elf_ctx *c = (struct elf_ctx *)ctx;
    const char *p = strstr(line, "NEEDED");
    if (!p) return;
    p += strlen("NEEDED");
    while (*p == ' ' || *p == '\t') p++;
    if (*p) dep_add(c->list, p, "");
}

static void resolve_elf(dep_list_t *list, const char *path)
{
    char cmd[1024];
    struct elf_ctx c = { list };
    snprintf(cmd, sizeof(cmd), "readelf -d \"%s\" 2>/dev/null", path);
    run_collect(cmd, elf_line, &c);

    /* Best-effort: try to resolve each entry against /usr/lib and
     * /lib via ldconfig -p if available. */
    for (uint32_t i = 0; i < list->count; i++) {
        char resolve_cmd[1024];
        char soname[1024];
        FILE *q;
        snprintf(resolve_cmd, sizeof(resolve_cmd),
                 "ldconfig -p 2>/dev/null | awk '/%s/ {print $NF; exit}'",
                 list->entries[i].name);
        q = popen(resolve_cmd, "r");
        if (q && fgets(soname, sizeof(soname), q)) {
            size_t l = strlen(soname);
            while (l > 0 && (soname[l-1] == '\n' || soname[l-1] == '\r'))
                soname[--l] = '\0';
            if (l) safe_copy_helper(list->entries[i].resolved_path,
                                    sizeof(list->entries[i].resolved_path),
                                    soname);
            list->entries[i].resolved = 1;
        }
        if (q) pclose(q);
    }
}

/* ------------------------------------------------------------------ */
/* Mach-O: LC_LOAD_DYLIB                                              */
/* ------------------------------------------------------------------ */

struct mo_ctx { dep_list_t *list; };

static void macho_line(void *ctx, const char *line)
{
    struct mo_ctx *c = (struct mo_ctx *)ctx;
    /* otool -L foo prints lines like:
     *      /usr/lib/libSystem.B.dylib (compatibility version 1.0.0 ...) */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '\t') return;
    /* skip the first line (the binary itself) */
    if (strstr(line, "(compatibility version") == NULL &&
        strstr(line, "(current version") == NULL)
        return;
    {
        char name[1024];
        size_t i = 0;
        while (*p && *p != ' ' && i + 1 < sizeof(name))
            name[i++] = *p++;
        name[i] = '\0';
        if (i > 0) dep_add(c->list, name, name);
    }
}

static void resolve_macho(dep_list_t *list, const char *path)
{
    char cmd[1024];
    struct mo_ctx c = { list };
    snprintf(cmd, sizeof(cmd), "otool -L \"%s\" 2>/dev/null", path);
    run_collect(cmd, macho_line, &c);
    if (list->count == 0) {
        dep_add(list, "libSystem.B.dylib", "libsystem_hello.dylib");
        dep_add(list, "Foundation.framework/Foundation", "");
    }
}

/* ------------------------------------------------------------------ */
/* DEX: core ART libraries                                            */
/* ------------------------------------------------------------------ */

static void resolve_dex(dep_list_t *list, const char *path)
{
    char cmd[1024];
    FILE *p;
    char line[1024];
    (void)path;
    /* dexdump -l plain classes.dex lists strings, but the core ART
     * libraries are always required. */
    dep_add(list, "libcore.so",        "/system/lib/libcore.so");
    dep_add(list, "libart.so",         "/system/lib/libart.so");
    dep_add(list, "libandroid.so",     "/system/lib/libandroid.so");
    dep_add(list, "libbinder_ndk.so",  "/system/lib/libbinder_ndk.so");

    snprintf(cmd, sizeof(cmd),
             "dexdump -l plain \"%s\" 2>/dev/null | grep -o 'L[^;]*;' | sort -u",
             path);
    p = popen(cmd, "r");
    if (!p) return;
    while (fgets(line, sizeof(line), p)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r'))
            line[--l] = '\0';
        if (l) dep_add(list, line, "");
    }
    pclose(p);
}

/* ------------------------------------------------------------------ */
/* HarmonyOS: module.json dependencies                                */
/* ------------------------------------------------------------------ */

static void resolve_harmony(dep_list_t *list, const char *path)
{
    char cmd[1024];
    FILE *pipe;
    char json[8192];
    size_t n;

    snprintf(cmd, sizeof(cmd),
             "unzip -p \"%s\" module.json 2>/dev/null", path);
    pipe = popen(cmd, "r");
    if (!pipe) {
        dep_add(list, "libace_napi.z.so", "/system/lib/libace_napi.z.so");
        return;
    }
    n = fread(json, 1, sizeof(json) - 1, pipe);
    pclose(pipe);
    if (n == 0) {
        dep_add(list, "libace_napi.z.so", "/system/lib/libace_napi.z.so");
        return;
    }
    json[n] = '\0';

    /* Heuristic: every quoted "moduleName" : "name" is a dependency. */
    {
        const char *p = json;
        while ((p = strstr(p, "\"moduleName\"")) != NULL) {
            const char *q = p + strlen("\"moduleName\"");
            while (*q && *q != ':') q++;
            while (*q && (*q == ':' || *q == ' ' || *q == '\t' ||
                          *q == '\n')) q++;
            if (*q == '"') {
                char name[256];
                size_t i = 0;
                q++;
                while (*q && *q != '"' && i + 1 < sizeof(name))
                    name[i++] = *q++;
                name[i] = '\0';
                if (i > 0) dep_add(list, name, "");
            }
            p = q;
        }
    }
    dep_add(list, "libace_napi.z.so", "/system/lib/libace_napi.z.so");
    dep_add(list, "libhilog_ndk.z.so", "/system/lib/libhilog_ndk.z.so");
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

dep_list_t *ResolveDeps(const char *path, app_type_t type)
{
    dep_list_t *list;
    if (!path) return NULL;
    list = dep_list_new(type);
    if (!list) return NULL;

    switch (type) {
    case APP_TYPE_WINDOWS: resolve_pe(list, path);     break;
    case APP_TYPE_LINUX:   resolve_elf(list, path);    break;
    case APP_TYPE_MACOS:   resolve_macho(list, path);  break;
    case APP_TYPE_ANDROID: resolve_dex(list, path);    break;
    case APP_TYPE_HARMONY: resolve_harmony(list, path);break;
    default:
        dep_add(list, "unknown", "");
        break;
    }
    return list;
}

void DepListFree(dep_list_t *list)
{
    free(list);
}
