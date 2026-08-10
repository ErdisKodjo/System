/*
 * dex2oat/compiler_driver.cc — Drives DEX → native compilation.
 *
 * The CompilerDriver walks every class & method in a .dex file, invokes
 * the backend (optimizing compiler or dex-to-native fast path) on each
 * method, and writes the resulting .oat (an ELF shared object) to disk.
 *
 * The sandbox implementation:
 *   - Parses the .dex header (just enough to know how many classes/methods).
 *   - Iterates classes via ClassLinkerForEachMethod.
 *   - Calls the optimizing backend for "speed" / "speed-profile" filters,
 *     or the dex-to-native fast path for "quicken".
 *   - Writes a minimal ELF stub to the output path so the loader can
 *     recognise it as a valid .oat.
 *
 * The driver state (current filter, accumulated stats) is global; dex2oat
 * is a short-lived process so this is fine.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <atomic>
#include <mutex>

extern "C" {
    void ClassLinkerForEachMethod(int (*cb)(const char *cls,
                                            const char *m,
                                            const char *sig,
                                            void *ctx),
                                  void *ctx);
    status_t OptimizingCompile(const char *cls, const char *method,
                               const char *sig, void **out_code);
    status_t DexToNativeCompile(const char *cls, const char *method,
                                const char *sig, void **out_code);
}

/* DEX header — only the fields we actually read. */
struct DexHeader {
    uint8_t  magic[8];     /* "dex\n035\0" */
    uint32_t checksum;
    uint8_t  signature[20];
    uint32_t file_size;
    uint32_t header_size;
    uint32_t endian_tag;
    uint32_t link_size;
    uint32_t link_off;
    uint32_t map_off;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
};

struct DriverStats {
    std::atomic<uint32_t> classes{0};
    std::atomic<uint32_t> methods{0};
    std::atomic<uint32_t> compiled{0};
    std::atomic<uint32_t> skipped{0};

    void reset() {
        classes.store(0);
        methods.store(0);
        compiled.store(0);
        skipped.store(0);
    }
};

static std::mutex g_lock;
static std::string g_filter = "speed";
static DriverStats g_stats;

extern "C" status_t CompilerDriverSetFilter(const char *filter) {
    if (!filter) return BAD_VALUE;
    static const char *kKnown[] = {
        "assume-verified", "verify", "quicken", "speed", "speed-profile",
        "everything", nullptr,
    };
    for (size_t i = 0; kKnown[i]; i++) {
        if (std::strcmp(filter, kKnown[i]) == 0) {
            std::lock_guard<std::mutex> lk(g_lock);
            g_filter = filter;
            return OK;
        }
    }
    return BAD_VALUE;
}

/* For each method: pick the backend based on the filter and "compile" it. */
static int on_method(const char *cls, const char *m, const char *sig, void *ctx) {
    g_stats.methods++;
    void *code = nullptr;
    status_t s;
    bool quicken_only;
    {
        std::lock_guard<std::mutex> lk(g_lock);
        quicken_only = (g_filter == "quicken" || g_filter == "verify" ||
                        g_filter == "assume-verified");
    }
    if (quicken_only) {
        s = DexToNativeCompile(cls, m, sig, &code);
    } else {
        s = OptimizingCompile(cls, m, sig, &code);
    }
    if (s == OK) g_stats.compiled++;
    else         g_stats.skipped++;
    (void)ctx;
    return 0; /* keep iterating */
}

/* Minimal ELF writer — produces a valid (but tiny) .oat file. */
static status_t write_elf_stub(const char *path, uint32_t classes,
                               uint32_t methods) {
    FILE *f = std::fopen(path, "wb");
    if (!f) return NO_INIT;
    /* 64-bit ELF header. */
    unsigned char ident[16] = { 0x7f,'E','L','F', 2, 1, 1, 0,
                                0,0,0,0,0,0,0,0 };
    std::fwrite(ident, 1, 16, f);
    uint16_t type = 1;   /* ET_REL */
    uint16_t machine = 183; /* EM_AARCH64 */
    uint32_t version = 1;
    uint64_t entry = 0;
    uint64_t phoff = 0;
    uint64_t shoff = 0;
    uint32_t flags = 0;
    uint16_t ehsize = 64;
    uint16_t phentsize = 0, phnum = 0;
    uint16_t shentsize = 0, shnum = 0, shstrndx = 0;
    std::fwrite(&type, 2, 1, f);
    std::fwrite(&machine, 2, 1, f);
    std::fwrite(&version, 4, 1, f);
    std::fwrite(&entry, 8, 1, f);
    std::fwrite(&phoff, 8, 1, f);
    std::fwrite(&shoff, 8, 1, f);
    std::fwrite(&flags, 4, 1, f);
    std::fwrite(&ehsize, 2, 1, f);
    std::fwrite(&phentsize, 2, 1, f);
    std::fwrite(&phnum, 2, 1, f);
    std::fwrite(&shentsize, 2, 1, f);
    std::fwrite(&shnum, 2, 1, f);
    std::fwrite(&shstrndx, 2, 1, f);
    /* Trailer with our stats so a reader can sanity-check. */
    char trailer[64];
    std::memset(trailer, 0, sizeof(trailer));
    std::snprintf(trailer, sizeof(trailer),
                  "AFROS-OAT1 classes=%u methods=%u", classes, methods);
    std::fwrite(trailer, 1, sizeof(trailer), f);
    std::fclose(f);
    return OK;
}

/* Try to read the class_defs_size field of a .dex; on failure return 0. */
static uint32_t read_dex_class_count(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (!f) return 0;
    DexHeader h;
    size_t n = std::fread(&h, 1, sizeof(h), f);
    std::fclose(f);
    if (n < sizeof(h)) return 0;
    if (std::memcmp(h.magic, "dex\n", 4) != 0) return 0;
    return h.class_defs_size;
}

extern "C" status_t CompilerDriverCompile(const char *dex_path,
                                          const char *out_path,
                                          const char * /*isa*/,
                                          const char * /*filter*/) {
    if (!dex_path || !out_path) return BAD_VALUE;
    g_stats.reset();
    uint32_t class_count = read_dex_class_count(dex_path);
    g_stats.classes = class_count;
    ClassLinkerForEachMethod(on_method, nullptr);
    return write_elf_stub(out_path, class_count, g_stats.methods.load());
}

extern "C" void CompilerDriverStats(uint32_t *classes, uint32_t *methods,
                                    uint32_t *compiled, uint32_t *skipped) {
    if (classes)  *classes  = g_stats.classes.load();
    if (methods)  *methods  = g_stats.methods.load();
    if (compiled) *compiled = g_stats.compiled.load();
    if (skipped)  *skipped  = g_stats.skipped.load();
}
